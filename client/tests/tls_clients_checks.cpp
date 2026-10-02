#include "client_core.h"
#include "admin_client.h"
#include "encoder/bytes.h"
#include <filesystem>
#include <iostream>
#include <cstdlib>

void check(bool passed, const std::string& message) {
  if (!passed) { std::cerr << message << '\n'; std::exit(1); }
}

int main(int argc, char** argv) {
  check(argc == 6, "Usage: tls-clients-checks certificate client-port admin-port directory host");
  encoder::ClientConfig config;
  config.host = argv[5];
  config.port = std::stoi(argv[2]);
  config.ca_file = argv[1];
  encoder::ClientCore client(config);
  std::string error;
  unsigned permissions = 0;
  check(client.authenticate("tester", "test-password", &error, &permissions), "Login: " + error);
  check(permissions == 3, "Server permissions");

  encoder::AdminConfig admin_config;
  admin_config.ca_file = argv[1];
  encoder::AdminClient admin(admin_config);
  encoder::AdminDevice device{"Local test", config.host, std::stoi(argv[3]), "test-token"};
  encoder::Header command;
  command.set("op", "admin_list_users");
  std::string payload;
  check(admin.user_command(device, command, &payload, &error), "Admin list: " + error);
  check(payload.find("tester") != std::string::npos, "Admin user list contains tester");
  device.token = "invalid-token";
  check(!admin.user_command(device, command, &payload, &error), "Reject invalid admin token");
  device.token = "test-token";

  const std::vector<uint8_t> original{'M', 'a', 'c', 0, 255, '\n', 42};
  for (const std::string storage : {"server", "client"}) {
    const auto path = (std::filesystem::u8path(argv[4]) /
                      std::filesystem::u8path("Проверка macOS " + storage + ".bin")).u8string();
    check(encoder::write_file(path, original), "Write Unicode test file");
    encoder::EncryptParams params{"tester", "test-password", path,
        encoder::Cipher::AES_256_GCM, encoder::HashAlg::SHA256, storage};
    encoder::EncryptResult encrypted;
    check(client.encrypt_file(params, &encrypted), "Encrypt: " + encrypted.message);
    bool read_ok = false;
    check(encoder::read_file(path, &read_ok) != original && read_ok, "File replaced with ciphertext");
    encoder::DecryptResult decrypted;
    check(client.decrypt_file({"tester", "test-password", path}, &decrypted),
          "Decrypt: " + decrypted.message);
    check(decrypted.data.to_vector() == original, "Exact binary round trip with " + storage + " keys");
  }

  command.set("op", "admin_set_permissions");
  command.set("username", "tester");
  command.set("permissions", "0");
  check(admin.user_command(device, command, &payload, &error), "Admin changes permissions");
  encoder::EncryptParams forbidden{"tester", "test-password", "unused",
      encoder::Cipher::AES_256_GCM, encoder::HashAlg::SHA256, "server"};
  encoder::EncryptResult denied;
  check(!client.encrypt_data(forbidden, original, &denied, false), "Server rejects forbidden encryption");

  auto wrong_hostname = config;
  wrong_hostname.host = config.host == "localhost" ? "127.0.0.1" : "localhost";
  encoder::ClientCore untrusted(wrong_hostname);
  check(!untrusted.authenticate("tester", "test-password", &error), "Reject wrong certificate hostname");
  device.host = wrong_hostname.host;
  command.set("op", "admin_list_users");
  check(!admin.user_command(device, command, &payload, &error), "Admin rejects wrong certificate hostname");
  std::cout << "Client TLS login, Unicode binary round trips, admin commands, permissions and certificate rejection passed\n";
}
