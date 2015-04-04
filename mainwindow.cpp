#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "dialog.h"
#include "util.h"
#include <QClipboard>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>

/**
 * @brief MainWindow::MainWindow
 * @param parent
 */
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    process(new QProcess(this))
{
//    connect(process.data(), SIGNAL(readyReadStandardOutput()), this, SLOT(readyRead()));
    connect(process.data(), SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
    connect(process.data(), SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(processFinished(int, QProcess::ExitStatus)));
    ui->setupUi(this);
    enableUiElements(true);
}

/**
 * @brief MainWindow::~MainWindow
 */
MainWindow::~MainWindow()
{
}

void MainWindow::normalizePassStore() {
    if (!passStore.endsWith("/") && !passStore.endsWith(QDir::separator())) {
        passStore += '/';
    }
}

QSettings &MainWindow::getSettings() {
    if (!settings) {
        QString portable_ini = QCoreApplication::applicationDirPath() + "/qtpass.ini";
        if (QFile(portable_ini).exists()) {
            settings.reset(new QSettings(portable_ini, QSettings::IniFormat));
        } else {
            settings.reset(new QSettings("IJHack", "QtPass"));
        }
    }
    return *settings;
}

/**
 * @brief MainWindow::checkConfig
 */
void MainWindow::checkConfig() {

    QSettings &settings(getSettings());

    usePass = (settings.value("usePass") == "true");

    useClipboard = (settings.value("useClipboard") == "true");
    useAutoclear = (settings.value("useAutoclear") == "true");
    autoclearSeconds = settings.value("autoclearSeconds").toInt();
    hidePassword = (settings.value("hidePassword") == "true");
    hideContent = (settings.value("hideContent") == "true");

    passStore = settings.value("passStore").toString();
    if (passStore == "") {
        passStore = Util::findPasswordStore();
        settings.setValue("passStore", passStore);
    }
    normalizePassStore();

    passExecutable = settings.value("passExecutable").toString();
    if (passExecutable == "") {
        passExecutable = Util::findBinaryInPath("pass");
    }

    gitExecutable = settings.value("gitExecutable").toString();
    if (gitExecutable == "") {
        gitExecutable = Util::findBinaryInPath("git");
    }

    gpgExecutable = settings.value("gpgExecutable").toString();
    if (gpgExecutable == "") {
        gpgExecutable = Util::findBinaryInPath("gpg");
    }
    gpgHome = settings.value("gpgHome").toString();

    if (passExecutable == "" && (gitExecutable == "" || gpgExecutable == "")) {
        config();
    }

    model.setNameFilters(QStringList() << "*.gpg");
    model.setNameFilterDisables(false);

    proxyModel.setSourceModel(&model);
    proxyModel.setModelAndStore(&model, passStore);
    selectionModel.reset(new QItemSelectionModel(&proxyModel));
    model.fetchMore(model.setRootPath(passStore));
    model.sort(0, Qt::AscendingOrder);

    ui->treeView->setModel(&proxyModel);
    ui->treeView->setRootIndex(proxyModel.mapFromSource(model.setRootPath(passStore)));
    ui->treeView->setColumnHidden(1, true);
    ui->treeView->setColumnHidden(2, true);
    ui->treeView->setColumnHidden(3, true);
    ui->treeView->setHeaderHidden(true);
    ui->treeView->setIndentation(15);
    ui->treeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    ui->textBrowser->setOpenExternalLinks(true);

    ui->lineEdit->setFocus();
}

/**
 * @brief MainWindow::config
 */
void MainWindow::config() {
    QScopedPointer<Dialog> d(new Dialog());
    d->setModal(true);

    d->setPassPath(passExecutable);
    d->setGitPath(gitExecutable);
    d->setGpgPath(gpgExecutable);
    d->setStorePath(passStore);
    d->usePass(usePass);
    d->useClipboard(useClipboard);
    d->useAutoclear(useAutoclear);
    d->setAutoclear(autoclearSeconds);
    d->hidePassword(hidePassword);
    d->hideContent(hideContent);

    if (d->exec()) {
        if (d->result() == QDialog::Accepted) {
            passExecutable = d->getPassPath();
            gitExecutable = d->getGitPath();
            gpgExecutable = d->getGpgPath();
            passStore = d->getStorePath();
            normalizePassStore();
            usePass = d->usePass();
            useClipboard = d->useClipboard();
            useAutoclear = d->useAutoclear();
            autoclearSeconds = d->getAutoclear();
            hidePassword = d->hidePassword();
            hideContent = d->hideContent();

            QSettings &settings(getSettings());

            settings.setValue("passExecutable", passExecutable);
            settings.setValue("gitExecutable", gitExecutable);
            settings.setValue("gpgExecutable", gpgExecutable);
            settings.setValue("passStore", passStore);
            settings.setValue("usePass", usePass ? "true" : "false");
            settings.setValue("useClipboard", useClipboard ? "true" : "false");
            settings.setValue("useAutoclear", useAutoclear ? "true" : "false");
            settings.setValue("autoclearSeconds", autoclearSeconds);
            settings.setValue("hidePassword", hidePassword ? "true" : "false");
            settings.setValue("hideContent", hideContent ? "true" : "false");

            ui->treeView->setRootIndex(model.setRootPath(passStore));
        }
    }
}

/**
 * @brief MainWindow::on_updateButton_clicked
 */
void MainWindow::on_updateButton_clicked()
{
    ui->statusBar->showMessage(tr("Updating password-store"), 2000);
    currentAction = GIT;
    if (usePass) {
        executePass("git pull");
    } else {
        executeWrapper(gitExecutable, "pull");
    }
}

/**
 * @brief MainWindow::on_pushButton_clicked
 */
void MainWindow::on_pushButton_clicked()
{
    ui->statusBar->showMessage(tr("Updating password-store"), 2000);
    currentAction = GIT;
    if (usePass) {
        executePass("git push");
    } else {
        executeWrapper(gitExecutable, "push");
    }
}

QString MainWindow::getFile(const QModelIndex &index, bool forPass)
{
    if (!index.isValid() || !model.fileInfo(proxyModel.mapToSource(index)).isFile()) {
        return QString();
    }
    QString filePath = model.filePath(proxyModel.mapToSource(index));
    if (forPass) {
        filePath.replace(QRegExp("\\.gpg$"), "");
        filePath.replace(QRegExp("^" + passStore), "");
    }
    return filePath;
}

/**
 * @brief MainWindow::on_treeView_clicked
 * @param index
 */
void MainWindow::on_treeView_clicked(const QModelIndex &index)
{
    lastDecrypt = "Could not decrypt";
    QString file = getFile(index, usePass);
    if (!file.isEmpty()){
        currentAction = GPG;
        if (usePass) {
            executePass('"' + file + '"');
        } else {
            executeWrapper(gpgExecutable , "--no-tty --use-agent -dq \"" + file + '"');
        }
    }
}

/**
 * @brief MainWindow::executePass
 * @param args
 */
void MainWindow::executePass(QString args, QString input) {
    executeWrapper(passExecutable, args, input);
}

/**
 * @brief MainWindow::executeWrapper
 * @param app
 * @param args
 */
void MainWindow::executeWrapper(QString app, QString args, QString input) {
    process->setWorkingDirectory(passStore);
    if (!gpgHome.isEmpty()) {
        QStringList env = QProcess::systemEnvironment();
        QDir absHome(gpgHome);
        absHome.makeAbsolute();
        env << "GNUPGHOME=" + absHome.path();
        process->setEnvironment(env);
    }
    process->start('"' + app + "\" " + args);
    ui->textBrowser->clear();
    ui->textBrowser->setTextColor(Qt::black);
    enableUiElements(false);
    if (!input.isEmpty()) {
        process->write(input.toUtf8());
    }
    process->closeWriteChannel();
}

/**
 * @brief MainWindow::readyRead
 */
void MainWindow::readyRead(bool finished = false) {
    QString output = ui->textBrowser->document()->toPlainText();
    QString error = process->readAllStandardError();
    if (error.size() > 0) {
        ui->textBrowser->setTextColor(Qt::red);
        output += error;
    }
    output += process->readAllStandardOutput();
    if (finished && currentAction == GPG) {
        lastDecrypt = output;
        if (useClipboard) {
            QClipboard *clip = QApplication::clipboard();
            QStringList tokens =  output.split("\n");
            clip->setText(tokens[0]);
            ui->statusBar->showMessage(tr("Password copied to clipboard"), 3000);
            if (useAutoclear) {
                  clippedPass = tokens[0];
                  QTimer::singleShot(1000*autoclearSeconds, this, SLOT(clearClipboard()));
            }
            if (hidePassword) {
                tokens.pop_front();
                output = tokens.join("\n");
            }
            if (hideContent) {
                output = tr("Content hidden");
            }
        }
    }
    output.replace(QRegExp("<"), "&lt;");
    output.replace(QRegExp(">"), "&gt;");
    output.replace(QRegExp("((http|https|ftp)\\://[a-zA-Z0-9\\-\\.]+\\.[a-zA-Z]{2,3}(:[a-zA-Z0-9]*)?/?([a-zA-Z0-9\\-\\._\\?\\,\\'/\\\\+&amp;%\\$#\\=~])*)"), "<a href=\"\\1\">\\1</a>");
    output.replace(QRegExp("\n"), "<br />");
    ui->textBrowser->setHtml(output);
}

/**
 * @brief MainWindow::clearClipboard
 * @TODO check clipboard content (only clear if contains the password)
 */
void MainWindow::clearClipboard()
{
    QClipboard *clipboard = QApplication::clipboard();
    if (clipboard->text() == clippedPass) {
        clipboard->clear();
        clippedPass = "";
        ui->statusBar->showMessage(tr("Clipboard cleared"), 3000);
    } else {
        ui->statusBar->showMessage(tr("Clipboard not cleared"), 3000);
    }
}

/**
 * @brief MainWindow::processFinished
 * @param exitCode
 * @param exitStatus
 */
void MainWindow::processFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    bool error = exitStatus != QProcess::NormalExit || exitCode > 0;
    if (error) {
         ui->textBrowser->setTextColor(Qt::red);
    }
    readyRead(true);
    enableUiElements(true);
    if (!error && currentAction == EDIT) {
        on_treeView_clicked(ui->treeView->currentIndex());
    }
}

/**
 * @brief MainWindow::enableUiElements
 * @param state
 */
void MainWindow::enableUiElements(bool state) {
    ui->updateButton->setEnabled(state);
    ui->treeView->setEnabled(state);
    ui->lineEdit->setEnabled(state);
    ui->addButton->setEnabled(state);
    state &= ui->treeView->currentIndex().isValid();
    ui->deleteButton->setEnabled(state);
    ui->editButton->setEnabled(state);
}

/**
 * @brief MainWindow::processError
 * @param error
 */
void MainWindow::processError(QProcess::ProcessError error)
{
    QString errorString;
    switch (error) {
        case QProcess::FailedToStart:
            errorString = tr("QProcess::FailedToStart");
            break;
        case QProcess::Crashed:
            errorString = tr("QProcess::Crashed");
            break;
        case QProcess::Timedout:
            errorString = tr("QProcess::Timedout");
            break;
        case QProcess::ReadError:
            errorString = tr("QProcess::ReadError");
            break;
        case QProcess::WriteError:
            errorString = tr("QProcess::WriteError");
            break;
        case QProcess::UnknownError:
            errorString = tr("QProcess::UnknownError");
            break;
    }
    ui->textBrowser->setTextColor(Qt::red);
    ui->textBrowser->setText(errorString);
    if (process->state() == QProcess::NotRunning)
        enableUiElements(true);
}

/**
 * @brief MainWindow::setPassExecutable
 * @param path
 */
void MainWindow::setPassExecutable(QString path) {
    passExecutable = path;
}

/**
 * @brief MainWindow::setGitExecutable
 * @param path
 */
void MainWindow::setGitExecutable(QString path) {
    gitExecutable = path;
}

/**
 * @brief MainWindow::setGpgExecutable
 * @param path
 */
void MainWindow::setGpgExecutable(QString path) {
    gpgExecutable = path;
}

/**
 * @brief MainWindow::on_configButton_clicked
 */
void MainWindow::on_configButton_clicked()
{
    config();
}

/**
 * @brief MainWindow::on_lineEdit_textChanged
 * @param arg1
 */
void MainWindow::on_lineEdit_textChanged(const QString &arg1)
{
    ui->treeView->expandAll();
    ui->statusBar->showMessage(tr("Looking for: ") + arg1, 1000);
    QString query = arg1;
    query.replace(QRegExp(" "), ".*");
    QRegExp regExp(query, Qt::CaseInsensitive);
    proxyModel.setFilterRegExp(regExp);
    ui->treeView->setRootIndex(proxyModel.mapFromSource(model.setRootPath(passStore)));
    selectFirstFile();
}

/**
 * @brief MainWindow::on_lineEdit_returnPressed
 */
void MainWindow::on_lineEdit_returnPressed()
{
    selectFirstFile();
    on_treeView_clicked(ui->treeView->currentIndex());
}

/**
 * @brief MainWindow::selectFirstFile
 */
void MainWindow::selectFirstFile()
{
    QModelIndex index = proxyModel.mapFromSource(model.setRootPath(passStore));
    index = firstFile(index);
    ui->treeView->setCurrentIndex(index);
}

/**
 * @brief MainWindow::firstFile
 * @param parentIndex
 * @return QModelIndex
 */
QModelIndex MainWindow::firstFile(QModelIndex parentIndex) {
    QModelIndex index = parentIndex;
    int numRows = proxyModel.rowCount(parentIndex);
    for (int row = 0; row < numRows; ++row) {
        index = proxyModel.index(row, 0, parentIndex);
        if (model.fileInfo(proxyModel.mapToSource(index)).isFile()) {
            return index;
        }
        if (proxyModel.hasChildren(index)) {
            return firstFile(index);
        }
    }
    return index;
}

/**
 * @brief MainWindow::on_clearButton_clicked
 */
void MainWindow::on_clearButton_clicked()
{
    ui->lineEdit->clear();
}

void MainWindow::setPassword(QString file, bool overwrite)
{
    bool ok;
#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
    QString newValue = QInputDialog::getMultiLineText(this, tr("New Value"),
        tr("New password value:"),
        lastDecrypt, &ok);
#else
    QString newValue = QInputDialog::getText(this, tr("New Value"),
        tr("New password value:"), QLineEdit::Normal,
        lastDecrypt, &ok);
#endif
    if (!ok || newValue.isEmpty()) {
        return;
    }
    currentAction = EDIT;
    if (usePass) {
        QString force(overwrite ? " -f " : " ");
        executePass("insert" + force + "-m \"" + file + '"', newValue);
    } else {
        QFile gpgId(passStore + ".gpg-id");
        if (!gpgId.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::critical(this, tr("Can not edit"),
                tr("Password store lacks .gpg-id specifying encryption key"));
            return;
        }
        QString recipients;
        while (!gpgId.atEnd()) {
            QString recipient(gpgId.readLine());
            recipient = recipient.trimmed();
            if (!recipient.isEmpty()) {
                recipients += " -r \"" + recipient + '"';
            }
        }
        if (recipients.isEmpty()) {
            QMessageBox::critical(this, tr("Can not edit"),
                tr("Could not read encryption key to use"));
            return;
        }
        QString force(overwrite ? " --yes " : " ");
        executeWrapper(gpgExecutable , force + "--batch -eq --output \"" + file + "\" " + recipients + " -", newValue);
    }
}

void MainWindow::on_addButton_clicked()
{
    bool ok;
    QString file = QInputDialog::getText(this, tr("New file"),
        tr("New password file:"), QLineEdit::Normal,
        "", &ok);
    if (!ok || file.isEmpty()) {
        return;
    }
    file += ".gpg";
    lastDecrypt = "";
    setPassword(file, false);
    executeWrapper(gitExecutable, "add " + file);
//    executeWrapper(gitExecutable, "commit -a -m \"Adding " + file + "\"");
}

void MainWindow::on_deleteButton_clicked()
{
    QString file = getFile(ui->treeView->currentIndex(), usePass);
    if (QMessageBox::question(this, tr("Delete password?"),
        tr("Are you sure you want to delete %1").arg(file),
        QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    currentAction = DELETE;
    if (usePass) {
        executePass("rm -f \"" + file + '"');
    } else {
        QFile(file).remove();
    }
}

void MainWindow::on_editButton_clicked()
{
    QString file = getFile(ui->treeView->currentIndex(), usePass);
    if (file.isEmpty()) {
        QMessageBox::critical(this, tr("Can not edit"),
            tr("Selected password file does not exist, not able to edit"));
        return;
    }
    setPassword(file, true);
}

/**
 * @brief MainWindow::setApp
 * @param app
 */
void MainWindow::setApp(SingleApplication *app)
{
    connect(app, SIGNAL(messageAvailable(QString)), this, SLOT(messageAvailable(QString)));
}

/**
 * @brief MainWindow::messageAvailable
 * @param message
 */
void MainWindow::messageAvailable(QString message)
{
    if (message == "show") {
        ui->lineEdit->selectAll();
        ui->lineEdit->setFocus();
    } else {
        ui->treeView->expandAll();
        ui->lineEdit->setText(message);
        on_lineEdit_returnPressed();
    }
    show();
    raise();
}
