#include "network_dialog.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QDateTime>
#include <atomic>
#include <memory>
#include <thread>

NetworkDialog::NetworkDialog(const QString& name, Request request, QWidget* parent,
                             std::function<WifiConnectRequest()> connect_factory) : QDialog(parent) {
  setWindowTitle("Сеть платы — " + name);
  resize(940, 680);
  setMinimumSize(680, 500);
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(18, 18, 18, 18);
  layout->setSpacing(10);
  auto* explanation = new QLabel("Выберите сеть в списке или обновите данные платы.", this);
  explanation->setToolTip("Carrier — наличие связи, не проверка Интернета. Сохранённый список — кэш; поиск запускается отдельно.");
  explanation->setWordWrap(true);
  layout->addWidget(explanation);
  auto* view = new QPlainTextEdit(this);
  view->setObjectName("networkResult");
  view->setReadOnly(true);
  auto* results = new QTabWidget(this);
  results->addTab(view, "IP и интерфейсы");
  auto* status = new QLabel(this);
  status->setObjectName("networkStatus");
  status->setTextFormat(Qt::PlainText);
  status->setWordWrap(true);
  layout->addWidget(status);
  auto* filter = new QLineEdit(this);
  filter->setObjectName("wifiFilter");
  filter->setPlaceholderText("Поиск по названию сети или BSSID");
  filter->setClearButtonEnabled(true);
  filter->hide();
  layout->addWidget(filter);
  auto* summary = new QLabel(this);
  summary->setObjectName("wifiSummary");
  summary->setTextFormat(Qt::PlainText);
  summary->setWordWrap(true);
  layout->addWidget(summary);
  auto* connection = new QLabel(this);
  connection->setObjectName("wifiConnection");
  connection->setTextFormat(Qt::PlainText);
  connection->setWordWrap(true);
  connection->hide();
  layout->addWidget(connection);
  auto* networks = new QTableWidget(0, 5, this);
  networks->setObjectName("wifiNetworks");
  networks->setHorizontalHeaderLabels({"Сеть (SSID)", "Сигнал*", "МГц", "Защита", "BSSID"});
  networks->setEditTriggers(QAbstractItemView::NoEditTriggers);
  networks->setSelectionBehavior(QAbstractItemView::SelectRows);
  networks->verticalHeader()->hide();
  networks->verticalHeader()->setDefaultSectionSize(32);
  networks->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  networks->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  networks->setColumnWidth(1, 70);
  networks->setColumnWidth(2, 60);
  networks->setColumnWidth(3, 180);
  networks->setColumnWidth(4, 145);
  networks->setMinimumHeight(160);
  results->addTab(networks, "Сети Wi-Fi");
  layout->addWidget(results, 1);
  auto apply_filter = [=]() {
    const QString needle = filter->text().trimmed();
    for (int row = 0; row < networks->rowCount(); ++row) {
      const bool match = networks->item(row, 0)->text().contains(needle, Qt::CaseInsensitive) ||
                         networks->item(row, 4)->text().contains(needle, Qt::CaseInsensitive);
      networks->setRowHidden(row, !match);
    }
  };
  connect(filter, &QLineEdit::textChanged, this, [=]() { apply_filter(); });
  connect(networks->model(), &QAbstractItemModel::layoutChanged, this, [=]() { apply_filter(); });
  auto* refresh = new QPushButton("IP и интерфейсы", this);
  refresh->setObjectName("refreshNetwork");
  auto* wifi = new QPushButton("Последний список Wi-Fi", this);
  wifi->setObjectName("refreshWifi");
  auto* actions = new QGridLayout();
  actions->setHorizontalSpacing(10);
  actions->setVerticalSpacing(8);
  actions->setColumnStretch(0, 1);
  actions->setColumnStretch(1, 1);
  actions->addWidget(refresh, 0, 0);
  actions->addWidget(wifi, 0, 1);
  auto* current = new QPushButton("Текущее подключение", this);
  current->setObjectName("refreshWifiStatus");
  actions->addWidget(current, 1, 0);
  auto* scan = new QPushButton("Найти сети Wi-Fi", this);
  scan->setObjectName("scanWifi");
  scan->setToolTip("Нужен новый сервер с разрешённым поиском. Одна попытка за 30 секунд.");
  actions->addWidget(scan, 1, 1);
  auto* configure = new QPushButton("Подключение к Wi-Fi…", this);
  configure->setObjectName("configureWifi");
  actions->addWidget(configure, 2, 0);
  for (auto* button : {refresh, wifi, current, scan})
    button->setProperty("secondaryAction", true);
  layout->addLayout(actions);
  connect(configure, &QPushButton::clicked, this, [=] {
    QString ssid;
    const int row = networks->currentRow();
    if (row >= 0 && !networks->isRowHidden(row)) {
      const auto raw = networks->item(row, 0)->data(Qt::UserRole).toString();
      // Scanner output may contain supplicant escape sequences. Never submit
      // display escapes as a literal SSID; require manual entry in that case.
      if (!raw.contains('\\')) ssid = raw;
    }
    WifiConnectDialog dialog(ssid, connect_factory ? connect_factory() : WifiConnectRequest{}, this);
    dialog.exec();
  });
  auto* close = new QDialogButtonBox(QDialogButtonBox::Close, this);
  close->button(QDialogButtonBox::Close)->setText("Закрыть");
  connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);
  actions->addWidget(close, 2, 1);
  struct Result {
    std::atomic<bool> done{false};
    bool ok = false;
    std::string text, error, operation;
  };
  struct State { std::shared_ptr<Result> pending; };
  auto state = std::make_shared<State>();
  auto* poll = new QTimer(this);
  poll->setInterval(50);
  auto start = [=](const char* operation) {
    if (state->pending) return;
    state->pending = std::make_shared<Result>();
    state->pending->operation = operation;
    refresh->setEnabled(false);
    wifi->setEnabled(false);
    current->setEnabled(false);
    scan->setEnabled(false);
    status->setText(std::string(operation) == "admin_wifi_scan"
        ? "Поиск Wi-Fi… Ожидаем завершения на плате (до 30 секунд). Закрытие окна не отменяет поиск."
        : "Запрос к плате… Предыдущие данные пока не обновлены. Окно можно закрыть.");
    std::thread([result = state->pending, request, op = std::string(operation)]() {
      try { result->ok = request(op, &result->text, &result->error); }
      catch (...) { result->error = "Network request failed unexpectedly"; }
      result->done.store(true);
    }).detach();
    poll->start();
  };
  connect(poll, &QTimer::timeout, this, [=]() {
    if (!state->pending || !state->pending->done.load()) return;
    poll->stop();
    const bool scan_result = state->pending->operation == "admin_wifi_scan";
    const bool wifi_result = scan_result || state->pending->operation == "admin_wifi_results";
    if (state->pending->ok && wifi_result &&
        state->pending->text.rfind("bssid / frequency / signal level / flags / ssid\n", 0) != 0) {
      state->pending->ok = false;
      state->pending->error = "Invalid Wi-Fi response format";
    }
    status->setText(state->pending->ok ? "Ответ получен в " + QDateTime::currentDateTime().toString("HH:mm:ss")
        : "Ошибка обновления: " + QString::fromStdString(state->pending->error) +
          "\nПредыдущие данные сохранены; они могут быть устаревшими.");
    if (state->pending->ok && scan_result)
      status->setText("Получено событие завершения поиска. Список обновлён в " + QDateTime::currentDateTime().toString("HH:mm:ss"));
    if (state->pending->ok && state->pending->operation == "admin_network_status") {
      view->setPlainText(QString::fromStdString(state->pending->text));
      results->setCurrentWidget(view);
    }
    if (state->pending->ok && state->pending->operation == "admin_wifi_status") {
      QString details = "Состояние на момент снимка (не проверка Интернета):\n";
      for (const auto& line : QString::fromStdString(state->pending->text).split('\n')) {
        const int split = line.indexOf('=');
        if (split < 0) continue;
        const auto key = line.left(split), value = line.mid(split + 1);
        if (key == "wpa_state") {
          const auto state_text = value == "COMPLETED" ? "Подключено" :
              value == "DISCONNECTED" ? "Отключено" : value == "SCANNING" ? "Поиск сети" : value;
          details += "Состояние: " + state_text + "\n";
        } else if (key == "ssid") details += "Сеть: " + value + "\n";
        else if (key == "bssid") details += "BSSID: " + value + "\n";
        else if (key == "ip_address") details += "IP: " + value + "\n";
      }
      connection->setText(details);
      connection->show();
    }
    if (state->pending->ok && wifi_result) {
      const auto lines = QString::fromStdString(state->pending->text).split('\n');
      networks->setSortingEnabled(false);
      networks->setRowCount(0);
      int skipped = 0;
      for (int i = 1; i < lines.size(); ++i) {
        if (lines[i].isEmpty()) continue;
        const auto fields = lines[i].split('\t');
        bool signal_ok = false, frequency_ok = false;
        if (fields.size() != 5) { ++skipped; continue; }
        const int signal = fields[2].toInt(&signal_ok);
        const int frequency = fields[1].toInt(&frequency_ok);
        if (!signal_ok || !frequency_ok || frequency <= 0 || networks->rowCount() >= 512) {
          ++skipped; continue;
        }
        const int row = networks->rowCount();
        networks->insertRow(row);
        const QStringList values{fields[4].isEmpty() ? "(скрытая сеть)" : fields[4],
                                 fields[2], fields[1], fields[3], fields[0]};
        for (int column = 0; column < 5; ++column)
          networks->setItem(row, column, new QTableWidgetItem(values[column]));
        networks->item(row, 1)->setData(Qt::DisplayRole, signal);
        networks->item(row, 0)->setData(Qt::UserRole, fields[4]);
        networks->item(row, 2)->setData(Qt::DisplayRole, frequency);
      }
      summary->setText((scan_result ? QString("После поиска: ") : QString("Последний список: ")) + QString::number(networks->rowCount()) +
          " сетей." + (skipped ? " Пропущено строк: " + QString::number(skipped) : QString()));
      summary->setToolTip("Список может быть неполным; сохранённые записи могут быть устаревшими.\n"
          "Сигнал указан в формате драйвера. Экранирование SSID сохранено. Лимит — 512 сетей.");
      networks->setSortingEnabled(true);
      networks->sortItems(1, Qt::DescendingOrder);
      apply_filter();
      filter->show();
      results->setCurrentWidget(networks);
    }
    state->pending.reset();
    refresh->setEnabled(true);
    wifi->setEnabled(true);
    current->setEnabled(true);
    scan->setEnabled(true);
  });
  connect(refresh, &QPushButton::clicked, this, [=]() { start("admin_network_status"); });
  connect(wifi, &QPushButton::clicked, this, [=]() { start("admin_wifi_results"); });
  connect(current, &QPushButton::clicked, this, [=]() { start("admin_wifi_status"); });
  connect(scan, &QPushButton::clicked, this, [=]() { start("admin_wifi_scan"); });
  start("admin_network_status");
}
