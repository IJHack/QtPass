// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "passworddialog.h"
#include "fieldlabel.h"
#include "filecontent.h"
#include "pass.h"
#include "passwordconfiguration.h"
#include "pathvalidator.h"
#include "qtpasssettings.h"
#include "totp.h"
#include "ui_passworddialog.h"
#include "util.h"
#include <algorithm>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QHash>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <utility>

#include "qtpasslogging.h"

namespace {
/// Dynamic property markOtpFieldEdited() sets on the line edit the user typed
/// in; normalizeOtpField() reads it from the field that is the OTP one.
const char kOtpEditedProperty[] = "qtpassOtpEdited";
} // namespace

/**
 * @brief PasswordDialog::PasswordDialog basic constructor.
 * @param passConfig configuration constant
 * @param parent
 */
PasswordDialog::PasswordDialog(PasswordConfiguration passConfig,
                               QWidget *parent)
    : QDialog(parent), ui(new Ui::PasswordDialog),
      m_passConfig(std::move(passConfig)), m_pass(QtPassSettings::getPass()) {
  // m_pass is captured once from the singleton here. This constructor is
  // only reached from tests; production always uses the two-arg overload
  // that receives an explicit Pass* with a stable, caller-controlled lifetime.
  m_templating = false;
  m_isNew = false;

  ui->setupUi(this);
  connect(ui->checkBoxShow, &QCheckBox::toggled, this,
          &PasswordDialog::setPasswordVisible);
  setupTemplateBox();
  setLength(m_passConfig.length);
  setPasswordCharTemplate(m_passConfig.selected);

  connect(m_pass, &Pass::finishedShow, this, &PasswordDialog::setPass);
}

/**
 * @brief PasswordDialog::PasswordDialog complete constructor.
 * @param file
 * @param isNew
 * @param parent pointer
 */
PasswordDialog::PasswordDialog(Pass *pass, const AppSettings &s, QString file,
                               const bool &isNew, QWidget *parent)
    : QDialog(parent), ui(new Ui::PasswordDialog), m_pass(pass),
      m_file(std::move(file)), m_isNew(isNew) {

  ui->setupUi(this);
  connect(ui->checkBoxShow, &QCheckBox::toggled, this,
          &PasswordDialog::setPasswordVisible);
  setupTemplateBox();
  // The name row only appears once setNewEntryLocation() says where a new
  // entry may go; an existing entry keeps its name.
  ui->nameRow->hide();

  setWindowTitle(isNew && m_file.isEmpty()
                     ? tr("New password")
                     : this->windowTitle() + " " + m_file);
  m_passConfig = s.passwordConfiguration;
  usePwgen(s.usePwgen);
  setTemplate(s.passTemplate, s.useTemplate);
  templateAll(s.templateAllFields);

  setLength(m_passConfig.length);
  setPasswordCharTemplate(m_passConfig.selected);

  connect(m_pass, &Pass::finishedShow, this, &PasswordDialog::setPass);
  connect(m_pass, &Pass::processErrorExit, this, &PasswordDialog::onShowError);
  connect(this, &PasswordDialog::accepted, this, &PasswordDialog::on_accepted);
  connect(this, &PasswordDialog::rejected, this, &PasswordDialog::on_rejected);

  if (!isNew) {
    // Show() is asynchronous: it forks gpg/pass and finishedShow only lands
    // once the event loop runs again, so right after construction the fields
    // are still empty. Clicking Ok in that window used to report Accepted
    // while writing nothing, and the late setPass() then overwrote whatever
    // was typed. Lock the editor and Ok until the decrypt arrives.
    ui->statusLabel->setText(tr("Decrypting…"));
    setEditorEnabled(false);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    m_pass->Show(m_file);
  }
}

/**
 * @brief PasswordDialog::~PasswordDialog basic destructor.
 */
PasswordDialog::~PasswordDialog() {
  // QDialog's destructor hides the window after this body ran, and hiding
  // takes the focus from a line edit, which emits editingFinished() into a
  // slot of the PasswordDialog part that is already gone. Cut those
  // connections while everything is still whole.
  for (QLineEdit *line : m_templateLines + m_otherLines) {
    disconnect(line, nullptr, this, nullptr);
  }
}

/**
 * @brief PasswordDialog::setPasswordVisible hide or show passwords.
 * @param show
 */
void PasswordDialog::setPasswordVisible(bool show) {
  if (show) {
    ui->lineEditPassword->setEchoMode(QLineEdit::Normal);
  } else {
    ui->lineEditPassword->setEchoMode(QLineEdit::Password);
  }
}

/**
 * @brief PasswordDialog::on_createPasswordButton_clicked generate a random
 * password.
 */
void PasswordDialog::on_createPasswordButton_clicked() {
  ui->widget->setEnabled(false);
  const int currentIndex = ui->passwordTemplateSwitch->currentIndex();
  if (currentIndex < 0 ||
      currentIndex >= static_cast<int>(PasswordConfiguration::CHARSETS_COUNT)) {
    ui->widget->setEnabled(true);
    return;
  }

  QString newPass = m_pass->generatePassword(
      static_cast<unsigned int>(ui->spinBox_pwdLength->value()),
      m_passConfig.Characters[static_cast<PasswordConfiguration::CharacterSet>(
          currentIndex)]);
  if (!newPass.isEmpty()) {
    ui->lineEditPassword->setText(newPass);
  }
  ui->widget->setEnabled(true);
}

/**
 * @brief PasswordDialog::on_accepted handle Ok click for QDialog
 */
void PasswordDialog::on_accepted() {
  // For an existing entry, refuse to save until its decrypted content has
  // loaded (Show is asynchronous). getPassword() always returns at least a
  // newline, so the previous isEmpty() check never fired and clicking OK early
  // overwrote the entry with the empty dialog fields.
  if (!m_isNew && !m_contentLoaded) {
    return;
  }

  // Canonicalise before serialising: getPassword() stays a dumb serializer.
  normalizeOtpField();

  QString newValue = getPassword();
  if (newValue.right(1) != "\n") {
    newValue += "\n";
  }

  m_pass->Insert(m_file, newValue, !m_isNew);
}

/**
 * @brief PasswordDialog::on_rejected handle Cancel click for QDialog
 */
void PasswordDialog::on_rejected() { setPassword(QString()); }

/**
 * @brief PasswordDialog::setNewEntryLocation show the folder picker and the
 * name field for a new entry.
 */
void PasswordDialog::setNewEntryLocation(const QString &storeRoot,
                                         const QStringList &folders,
                                         const QString &currentFolder) {
  m_storeRoot = QDir::cleanPath(storeRoot);
  ui->folderBox->clear();
  int current = 0;
  for (const QString &folder : folders) {
    const QString rel = QDir::cleanPath(folder) == QStringLiteral(".")
                            ? QString()
                            : QDir::cleanPath(folder);
    if (rel == QDir::cleanPath(currentFolder) ||
        (rel.isEmpty() &&
         QDir::cleanPath(currentFolder) == QStringLiteral("."))) {
      current = ui->folderBox->count();
    }
    ui->folderBox->addItem(rel.isEmpty() ? QStringLiteral("/") : rel + u'/',
                           rel);
  }
  ui->folderBox->setCurrentIndex(current);
  m_newEntryFolder = ui->folderBox->currentData().toString();
  ui->nameRow->show();
  connect(ui->nameEdit, &QLineEdit::textChanged, this,
          &PasswordDialog::validateNewEntry);
  connect(ui->folderBox, &QComboBox::currentIndexChanged, this,
          &PasswordDialog::validateNewEntry);
  validateNewEntry();
  ui->nameEdit->setFocus();
}

/**
 * @brief Resolve folder + typed name into an entry path, or say why not.
 */
auto PasswordDialog::resolveNewEntry(QString *problem) const -> QString {
  QString name = QDir::fromNativeSeparators(ui->nameEdit->text().trimmed());
  // The suffix belongs to the file, not the entry: Insert() appends it, so a
  // typed "github.gpg" would otherwise become github.gpg.gpg.
  if (name.endsWith(QStringLiteral(".gpg"), Qt::CaseInsensitive)) {
    name.chop(4);
  }
  if (name.isEmpty()) {
    *problem = tr("Give the entry a name.");
    return {};
  }
  if (name.endsWith(u'/')) {
    *problem = tr("A name cannot end in /.");
    return {};
  }
  const QString folder = ui->folderBox->currentData().toString();
  const QString rel =
      QDir::cleanPath(folder.isEmpty() ? name : folder + u'/' + name);
  const QString absolute = QDir(m_storeRoot).filePath(rel);
  if (rel.isEmpty() || rel == QStringLiteral(".") ||
      !PathValidator::isPathInStore(m_storeRoot, absolute)) {
    *problem = tr("That name would resolve outside the password store.");
    return {};
  }
  if (QFileInfo::exists(absolute + QStringLiteral(".gpg"))) {
    *problem = tr("An entry called %1 already exists.").arg(rel);
    return {};
  }
  if (QFileInfo(absolute).isDir()) {
    *problem = tr("%1 is a folder.").arg(rel);
    return {};
  }
  problem->clear();
  return rel;
}

/**
 * @brief Re-check the name after every keystroke and gate OK on it.
 */
void PasswordDialog::validateNewEntry() {
  QString problem;
  const QString rel = resolveNewEntry(&problem);
  ui->statusLabel->setText(problem);
  ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!rel.isEmpty());
}

void PasswordDialog::accept() {
  if (!m_storeRoot.isEmpty()) {
    QString problem;
    const QString rel = resolveNewEntry(&problem);
    if (rel.isEmpty()) {
      ui->statusLabel->setText(problem);
      return;
    }
    // A name like work/vpn may need its folder; gpg does not create it.
    const QString parent = QFileInfo(QDir(m_storeRoot).filePath(rel)).path();
    if (!QDir(parent).exists() && !QDir().mkpath(parent)) {
      ui->statusLabel->setText(
          tr("Could not create the folder %1.").arg(parent));
      return;
    }
    m_file = rel;
    setWindowTitle(tr("Password") + QStringLiteral(" ") + m_file);
  }
  QDialog::accept();
}

/**
 * @brief PasswordDialog::setPassword populate the (templated) fields.
 * @param password
 */
void PasswordDialog::setPassword(const QString &password) {
  // Which lines become fields is the user's choice, not ours: the template's
  // fields when templating is on, every `key: value` line only with "Template
  // all fields". Forcing allFields (#1138, for #132) turned every such line
  // in every store into a label-locked widget, also with templates off, and
  // took away plain-text editing of those lines (#1766). The same rule is
  // applied in MainWindow when rendering the entry.
  FileContent fileContent =
      FileContent::parse(password, m_templating ? m_fields : QStringList(),
                         m_templating && m_allFields);
  ui->lineEditPassword->setText(fileContent.getPassword());

  QWidget *previous = ui->checkBoxShow;
  // first set templated values
  NamedValues namedValues = fileContent.getNamedValues();
  for (QLineEdit *line : std::as_const(m_templateLines)) {
    line->setText(namedValues.takeValue(line->objectName()));
    previous = line;
  }
  // show remaining values (if there are)
  // Remove previously created dynamic widgets to prevent duplicates and leaks
  for (QLineEdit *line : std::as_const(m_otherLines)) {
    ui->formLayout->removeRow(line);
  }
  m_otherLines.clear();
  for (const NamedValue &nv : std::as_const(namedValues)) {
    auto *line = new QLineEdit();
    line->setObjectName(nv.name);
    line->setText(nv.value);
    auto *label = new FieldLabel(nv.name);
    label->setBuddy(line);
    ui->formLayout->addRow(label, line);
    setTabOrder(previous, line);
    m_otherLines.append(line);
    previous = line;
    // Both widgets are rows of the form; removeRow() deletes them together,
    // so a rename or removal that arrives afterwards finds no receiver.
    connect(label, &FieldLabel::renamed, this,
            [this, line, label](const QString &, const QString &to) {
              renameField(line, label, to);
            });
    connect(label, &FieldLabel::removeRequested, this,
            [this, line] { removeField(line); });
    // The visible way to remove the field; the context menu is the other.
    auto *remove = line->addAction(
        QIcon::fromTheme(QStringLiteral("edit-delete"),
                         QIcon(QStringLiteral(":/icons/edit-delete.svg"))),
        QLineEdit::TrailingPosition);
    remove->setObjectName(QStringLiteral("removeFieldAction"));
    remove->setToolTip(tr("Remove field"));
    connect(remove, &QAction::triggered, this,
            [this, line] { removeField(line); });
  }

  // setPlainText (not insertPlainText) so re-populating replaces the body
  // instead of appending a second copy, which would be saved back as
  // duplicated content.
  ui->plainTextEdit->setPlainText(fileContent.getRemainingData());

  // m_otherLines was just rebuilt, so the OTP field may be a new widget.
  hookOtpField();
}

void PasswordDialog::renameField(QLineEdit *line, FieldLabel *label,
                                 const QString &to) {
  QList<QLineEdit *> allLines(m_templateLines);
  allLines.append(m_otherLines);
  for (QLineEdit *other : std::as_const(allLines)) {
    if (other != line && other->objectName() == to) {
      ui->statusLabel->setText(tr("A field called %1 already exists.").arg(to));
      return;
    }
  }
  ui->statusLabel->clear();
  line->setObjectName(to);
  label->setText(to);
  // The field may have become, or stopped being, the OTP one.
  hookOtpField();
}

void PasswordDialog::removeField(QLineEdit *line) {
  m_otherLines.removeAll(line);
  // takeRow(), not removeRow(): the latter deletes the label and the line
  // here, while the label's contextMenuEvent() (with a QMenu on its stack,
  // parented to the label) or the line's action's triggered() is still
  // running. Out of the form now, deleted once the stack has unwound.
  const QFormLayout::TakeRowResult row = ui->formLayout->takeRow(line);
  // hide() takes the focus from a focused line, and that emits
  // editingFinished(): not into normalizeOtpField() for a field that is gone.
  disconnect(line, nullptr, this, nullptr);
  if (m_otpLine == line) {
    m_otpLine = nullptr;
  }
  for (QLayoutItem *item : {row.labelItem, row.fieldItem}) {
    if (item == nullptr) {
      continue;
    }
    if (QWidget *widget = item->widget()) {
      widget->hide();
      widget->deleteLater();
    }
    delete item;
  }
  hookOtpField();
}

/**
 * @brief PasswordDialog::otpLineEdit find the OTP configuration field.
 * @return the matching QLineEdit, or nullptr when there is none
 */
auto PasswordDialog::otpLineEdit() const -> QLineEdit * {
  QList<QLineEdit *> allLines(m_templateLines);
  allLines.append(m_otherLines);
  QLineEdit *firstMatch = nullptr;
  for (QLineEdit *line : std::as_const(allLines)) {
    if (!FileContent::isOtpFieldName(line->objectName())) {
      continue;
    }
    // Prefer a populated field. The default template creates an empty `OTP`
    // widget, and setPassword() fills template widgets by exact name, so an
    // entry storing `TOTP:` leaves that one empty and puts the real secret in
    // m_otherLines. Returning the empty template widget meant the actual value
    // was never validated or normalised.
    if (!line->text().trimmed().isEmpty()) {
      return line;
    }
    if (firstMatch == nullptr) {
      firstMatch = line;
    }
  }
  return firstMatch;
}

/**
 * @brief PasswordDialog::hookOtpField attach validation to the OTP field.
 *
 * setTemplate() and setPassword() both recreate the field widgets, so this is
 * called from each of them rather than once from the constructor.
 */
void PasswordDialog::hookOtpField() {
  // Drop the previous warning indicator. Only m_otherLines widgets are deleted
  // by removeRow(); a template widget survives, so simply forgetting the action
  // left it installed forever and the next validate added a second icon.
  //
  // m_otpWarning is a QPointer, so it is already null when the QLineEdit that
  // owned the action was one of the m_otherLines widgets setPassword() just
  // deleted — dereferencing a raw pointer here was a use-after-free.
  if (!m_otpWarning.isNull()) {
    if (auto *owner = qobject_cast<QWidget *>(m_otpWarning->parent())) {
      owner->removeAction(m_otpWarning);
    }
    delete m_otpWarning;
  }
  m_otpWarning = nullptr;

  QLineEdit *otp = otpLineEdit();
  // The field that was the OTP one may not be any more (renamed away, or
  // another one now comes first); its editingFinished() must not run the
  // normalisation of the field that is.
  if (!m_otpLine.isNull() && m_otpLine != otp) {
    disconnect(m_otpLine, nullptr, this, nullptr);
  }
  m_otpLine = otp;
  if (otp == nullptr) {
    return;
  }
  otp->setPlaceholderText(tr("otpauth:// URI or base32 secret"));
  connect(otp, &QLineEdit::textChanged, this, &PasswordDialog::validateOtpField,
          Qt::UniqueConnection);
  // textEdited, not textChanged: it fires only for user input, so programmatic
  // population does not count as the user having touched the field.
  connect(otp, &QLineEdit::textEdited, this,
          &PasswordDialog::markOtpFieldEdited, Qt::UniqueConnection);
  // Canonicalise as soon as the user leaves the field, so the value they end
  // up saving is the value they were shown.
  connect(otp, &QLineEdit::editingFinished, this,
          &PasswordDialog::normalizeOtpField, Qt::UniqueConnection);
  validateOtpField();
}

void PasswordDialog::markOtpFieldEdited() {
  if (QObject *line = sender()) {
    line->setProperty(kOtpEditedProperty, true);
  }
}

/**
 * @brief PasswordDialog::validateOtpField flag an unusable OTP value.
 */
void PasswordDialog::validateOtpField() {
  QLineEdit *otp = otpLineEdit();
  if (otp == nullptr) {
    return;
  }

  const QString text = otp->text().trimmed();
  const bool bad = !text.isEmpty() && !Totp::isValid(text);
  if (bad) {
    if (m_otpWarning.isNull()) {
      // Theme-aware indicator: no hardcoded colours and no extra layout row.
      m_otpWarning =
          otp->addAction(QIcon::fromTheme(QStringLiteral("dialog-warning")),
                         QLineEdit::TrailingPosition);
    }
    otp->setToolTip(tr("Invalid OTP secret"));
  } else {
    if (!m_otpWarning.isNull()) {
      otp->removeAction(m_otpWarning);
      delete m_otpWarning;
      m_otpWarning = nullptr;
    }
    otp->setToolTip(QString());
  }
}

/**
 * @brief PasswordDialog::normalizeOtpField rewrite the OTP field as a URI.
 *
 * Only rewrites a value that is unambiguously TOTP configuration: an
 * `otpauth://` URI, or something the user typed into this very field.
 *
 * Anything else is left byte-for-byte. Base32::sanitizeInput() maps 1 to L and
 * 8 to B, so a static backup code like `12345678` looks like valid base32 and
 * used to be silently rewritten as an otpauth URI and re-encrypted — on any OK,
 * even when the user only edited the password field and never touched this one.
 * A bare secret left alone still works: Totp::parse() accepts one.
 */
void PasswordDialog::normalizeOtpField() {
  QLineEdit *otp = otpLineEdit();
  if (otp == nullptr) {
    return;
  }
  const QString text = otp->text().trimmed();
  if (text.isEmpty()) {
    return;
  }
  if (!otp->property(kOtpEditedProperty).toBool() &&
      !FileContent::isOtpUriValue(text)) {
    return;
  }
  // fileName(), not completeBaseName(): the latter truncates at the last dot,
  // turning an entry called github.com into a label of "github". m_file already
  // has its .gpg suffix stripped by MainWindow::getFile().
  const QString canonical = Totp::normalize(text, QFileInfo(m_file).fileName());
  if (!canonical.isEmpty()) {
    otp->setText(canonical);
  }
}

/**
 * @brief PasswordDialog::getPassword  join the (templated) fields to a QString
 * for writing back.
 * @return collapsed password.
 */
auto PasswordDialog::getPassword() -> QString {
  QString passFile = ui->lineEditPassword->text() + "\n";
  QList<QLineEdit *> allLines(m_templateLines);
  allLines.append(m_otherLines);
  for (QLineEdit *line : std::as_const(allLines)) {
    QString text = line->text();
    if (text.isEmpty()) {
      continue;
    }
    passFile += line->objectName() + ": " + text + "\n";
  }
  passFile += ui->plainTextEdit->toPlainText();
  return passFile;
}

/**
 * @brief PasswordDialog::setTemplate set the template and create the fields.
 * @param rawFields
 */
void PasswordDialog::setTemplate(const QString &rawFields, bool useTemplate) {
  m_fields = rawFields.split('\n');
  m_templating = useTemplate;

  for (QLineEdit *line : std::as_const(m_templateLines)) {
    ui->formLayout->removeRow(line);
  }
  m_templateLines.clear();

  // Defensively remove all rows tracked in m_otherLines to prevent accumulation
  // when cycling templates or applying new templates after setPassword
  for (QLineEdit *line : std::as_const(m_otherLines)) {
    ui->formLayout->removeRow(line);
  }
  m_otherLines.clear();

  if (m_templating) {
    QWidget *previous = ui->checkBoxShow;
    for (const QString &field : std::as_const(m_fields)) {
      if (field.isEmpty()) {
        continue;
      }
      auto *line = new QLineEdit();
      auto *label = new QLabel(field);
      line->setObjectName(field);
      ui->formLayout->addRow(label, line);
      setTabOrder(previous, line);
      m_templateLines.append(line);
      previous = line;
    }
  }

  hookOtpField();
}

/**
 * @brief PasswordDialog::templateAll split every `key: value` line into a
 *        field, not only the template's.
 * @param templateAll
 */
void PasswordDialog::templateAll(bool templateAll) {
  m_allFields = templateAll;
}

/**
 * @brief PasswordDialog::setLength
 * PasswordDialog::setLength password length.
 * @param length
 */
void PasswordDialog::setLength(int length) {
  ui->spinBox_pwdLength->setValue(length);
}

/**
 * @brief PasswordDialog::setPasswordCharTemplate
 * PasswordDialog::setPasswordCharTemplate chose the template style.
 * @param templateIndex
 */
void PasswordDialog::setPasswordCharTemplate(int templateIndex) {
  ui->passwordTemplateSwitch->setCurrentIndex(templateIndex);
}

/**
 * @brief PasswordDialog::usePwgen
 * PasswordDialog::usePwgen don't use own password generator.
 * @param usePwgen
 */
void PasswordDialog::usePwgen(bool usePwgen) {
  m_usePwgen = usePwgen;
  ui->passwordTemplateSwitch->setDisabled(usePwgen);
  ui->label_characterset->setDisabled(usePwgen);
}

/**
 * @brief Hide the template row until setAvailableTemplates() fills it and
 *        make a pick in the box apply that template.
 */
void PasswordDialog::setupTemplateBox() {
  ui->label_template->hide();
  ui->templateBox->hide();
  connect(ui->templateBox, &QComboBox::currentTextChanged, this,
          [this](const QString &name) {
            // applyTemplate() rebuilds the field rows, so only act on a real
            // change; the box is also updated from applyTemplate() itself.
            if (name != m_currentTemplateName) {
              applyTemplate(name);
            }
          });
}

/**
 * @brief Set available templates from .templates file and apply default.
 *
 * Shows the template row with the names, selects @p defaultTemplate (or the
 * first name) and enables Ctrl+T to cycle through them.
 * @param templates Hash of template name to field list.
 * @param defaultTemplate Name of default template to select.
 */
void PasswordDialog::setAvailableTemplates(
    const QHash<QString, QStringList> &templates,
    const QString &defaultTemplate) {
  m_availableTemplates = templates;
  QStringList templateNames = templates.keys();
  if (templateNames.isEmpty()) {
    return;
  }
  std::sort(templateNames.begin(), templateNames.end());
  {
    const QSignalBlocker blocker(ui->templateBox);
    ui->templateBox->clear();
    ui->templateBox->addItems(templateNames);
  }
  ui->label_template->show();
  ui->templateBox->show();
  if (!m_templateShortcut) {
    m_templateShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_T), this,
                                       this, [this] { cycleTemplate(); });
  }
  QString selected = defaultTemplate;
  if (!templateNames.contains(selected)) {
    selected = templateNames.first();
  }
  applyTemplate(selected);
}

/**
 * @brief Apply a template by name.
 * @param templateName Name of template to apply.
 */
void PasswordDialog::applyTemplate(const QString &templateName) {
  auto it = m_availableTemplates.constFind(templateName);
  if (it != m_availableTemplates.constEnd()) {
    m_currentTemplateName = templateName;
    const QSignalBlocker blocker(ui->templateBox);
    ui->templateBox->setCurrentText(templateName);
    QString fields = it.value().join("\n");
    setTemplate(fields, true);
  }
}

/**
 * @brief Cycle to next template (Ctrl+T).
 */
void PasswordDialog::cycleTemplate() {
  if (m_availableTemplates.isEmpty()) {
    return;
  }
  QStringList names = m_availableTemplates.keys();
  std::sort(names.begin(), names.end());

  qsizetype currentIdx = names.indexOf(m_currentTemplateName);
  qsizetype nextIdx;
  if (currentIdx < 0) {
    nextIdx = 0;
  } else {
    nextIdx = (currentIdx + 1) % names.size();
  }
  applyTemplate(names.at(nextIdx));
}

/**
 * @brief Sets the password from pass show output.
 * @param output Output from pass show command
 */
void PasswordDialog::setPass(const QString &output, const QString &file) {
  // finishedShow names its file: a decrypt of any other entry (a tree click
  // that was still queued when the dialog opened) is not ours.
  if (!file.isEmpty() && file != m_file) {
    return;
  }
  setPassword(output);
  m_contentLoaded = true;
  // The decrypt landed: unlock the editor and Ok, and drop the "Decrypting…"
  // note so it cannot be mistaken for a warning.
  ui->statusLabel->clear();
  setEditorEnabled(true);
  ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
}

/**
 * @brief PasswordDialog::onShowError surface a decrypt failure in the dialog.
 * @param exitCode Ignored exit code of the failed process.
 * @param err Error output to present to the user.
 */
void PasswordDialog::onShowError(int /* exitCode */, const QString &err) {
  // React only while the dialog is still waiting for its own decrypt.
  // processErrorExit carries no request identity, but for an existing entry
  // the only Pass operation in flight is the Show() that opened this dialog.
  if (m_isNew || m_contentLoaded) {
    return;
  }
  // Keep the dialog open so the user can read why it is inert; Ok stays
  // locked and the only way out is Cancel. The old behaviour closed the
  // window silently, which looked like "nothing happened".
  ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
  setEditorEnabled(false);
  ui->statusLabel->setText(err);
}

/**
 * @brief PasswordDialog::setEditorEnabled lock or restore every editable field.
 * @param enabled true to restore the editor, false to grey it out.
 */
void PasswordDialog::setEditorEnabled(bool enabled) {
  ui->lineEditPassword->setEnabled(enabled);
  ui->createPasswordButton->setEnabled(enabled);
  ui->checkBoxShow->setEnabled(enabled);
  ui->passwordTemplateSwitch->setEnabled(enabled);
  ui->label_characterset->setEnabled(enabled);
  ui->spinBox_pwdLength->setEnabled(enabled);
  ui->plainTextEdit->setEnabled(enabled);
  for (QLineEdit *line : std::as_const(m_templateLines)) {
    line->setEnabled(enabled);
  }
  for (QLineEdit *line : std::as_const(m_otherLines)) {
    line->setEnabled(enabled);
  }
  // Re-apply the pwgen policy on the way back up, or the two widgets usePwgen()
  // locks would be spuriously re-enabled with the rest of the editor.
  if (enabled && m_usePwgen) {
    usePwgen(true);
  }
}
