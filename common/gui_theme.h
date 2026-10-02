#pragma once

#include <QApplication>
#include <QPalette>
#include <QColor>

namespace encoder {
inline void applyDarkTheme(QApplication& app) {
  app.setStyle("Fusion");
  QPalette p;
  p.setColor(QPalette::Window, QColor("#0d1117"));
  p.setColor(QPalette::WindowText, QColor("#e6edf3"));
  p.setColor(QPalette::Base, QColor("#0d1117"));
  p.setColor(QPalette::AlternateBase, QColor("#161b22"));
  p.setColor(QPalette::Text, QColor("#e6edf3"));
  p.setColor(QPalette::Button, QColor("#21262d"));
  p.setColor(QPalette::ButtonText, QColor("#e6edf3"));
  p.setColor(QPalette::Highlight, QColor("#1f6feb"));
  p.setColor(QPalette::HighlightedText, Qt::white);
  p.setColor(QPalette::ToolTipBase, QColor("#161b22"));
  p.setColor(QPalette::ToolTipText, QColor("#e6edf3"));
  p.setColor(QPalette::PlaceholderText, QColor("#8b949e"));
  p.setColor(QPalette::Disabled, QPalette::Text, QColor("#6e7681"));
  p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#6e7681"));
  app.setPalette(p);
  app.setStyleSheet(R"(
    QWidget { font-family: "Segoe UI"; font-size: 13px; }
    QDialog, QMainWindow { background: #0d1117; }
    QGroupBox { background: #161b22; border: 1px solid #30363d;
      border-radius: 8px; margin-top: 24px; padding: 12px; }
    QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left;
      left: 12px; top: 0px; padding: 0 6px; color: #8b949e; }
    QLabel#headerTitle { font-size: 20px; font-weight: 600; color: #e6edf3; }
    QLabel#headerSub { color: #8b949e; margin-bottom: 6px; }
    QLineEdit, QComboBox, QSpinBox { background: #0d1117; color: #e6edf3;
      border: 1px solid #30363d; border-radius: 6px; padding: 5px 8px; }
    QLineEdit:focus, QComboBox:focus { border: 1px solid #58a6ff; }
    QComboBox { combobox-popup: 0; padding-right: 24px; }
    QComboBox QAbstractItemView { background: #161b22; color: #e6edf3;
      border: 1px solid #30363d; selection-background-color: #1f6feb;
      selection-color: white; outline: 0; }
    QComboBox QAbstractItemView::item { min-height: 26px; }
    QListWidget, QTableWidget, QTreeWidget, QPlainTextEdit, QTextEdit {
      background: #0d1117; color: #e6edf3; border: 1px solid #30363d;
      border-radius: 8px; padding: 4px; selection-background-color: #1f6feb; }
    QListWidget::item, QTreeWidget::item { padding: 5px 6px; min-height: 22px; }
    QScrollBar:vertical { background: #0d1117; width: 10px;
      margin: 8px 2px 8px 0; border: none; }
    QScrollBar:horizontal { background: #0d1117; height: 10px;
      margin: 0 8px 2px 8px; border: none; }
    QScrollBar::handle { background: #484f58; border-radius: 4px; }
    QScrollBar::handle:vertical { min-height: 28px; }
    QScrollBar::handle:horizontal { min-width: 28px; }
    QScrollBar::handle:hover { background: #6e7681; }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
    QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
    QAbstractScrollArea::corner { background: #0d1117; border: none; }
    QHeaderView::section { background: #21262d; color: #e6edf3;
      border: 1px solid #30363d; padding: 6px; }
    QPushButton { background: #238636; color: white; border: 1px solid #2ea043;
      border-radius: 6px; padding: 6px 12px; }
    QPushButton:hover { background: #2ea043; }
    QPushButton:pressed { background: #196c2e; }
    QPushButton:focus { border: 1px solid #58a6ff; }
    QPushButton#secondary { background: #21262d; color: #e6edf3; border-color: #30363d; }
    QPushButton#secondary:hover { background: #30363d; }
    QPushButton[secondaryAction="true"] { background: #21262d; color: #e6edf3; border-color: #30363d; }
    QPushButton[secondaryAction="true"]:hover { background: #30363d; }
    QPushButton#danger { background: #21262d; color: #f85149; border-color: #30363d; }
    QPushButton#danger:hover { background: #542426; }
    QPushButton:disabled { background: #161b22; color: #6e7681; border-color: #30363d; }
    QToolButton { color: #58a6ff; background: transparent; border: none; padding: 4px; }
    QCheckBox { spacing: 8px; padding: 2px; }
    QTabWidget::pane { border: 1px solid #30363d; }
    QTabBar::tab { background: #161b22; padding: 8px 14px; }
    QTabBar::tab:selected { background: #21262d; border-bottom: 2px solid #58a6ff; }
    QToolTip { color: #e6edf3; background: #161b22; border: 1px solid #30363d; }
  )");
}
} // namespace encoder
