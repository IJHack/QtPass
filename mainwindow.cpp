#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "dialog.h"
#include "usersdialog.h"
#include "util.h"
#include <QClipboard>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>
#ifdef Q_OS_WIN
#include <windows.h>
#undef DELETE
#endif

/**
 * @brief MainWindow::MainWindow
 * @param parent
 */
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    process(new QProcess(this)),
    fusedav(this)
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
#ifdef Q_OS_WIN
    if (useWebDav) WNetCancelConnection2A(passStore.toUtf8().constData(), 0, 1);
#else
    if (fusedav.state() == QProcess::Running) {
        fusedav.terminate();
        fusedav.waitForFinished(2000);
    }
#endif
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

void MainWindow::mountWebDav() {
#ifdef Q_OS_WIN
    char dst[20] = {0};
    NETRESOURCEA netres;
    memset(&netres, 0, sizeof(netres));
    netres.dwType = RESOURCETYPE_DISK;
    netres.lpLocalName = 0;
    netres.lpRemoteName = webDavUrl.toUtf8().data();
    DWORD size = sizeof(dst);
    DWORD r = WNetUseConnectionA(reinterpret_cast<HWND>(effectiveWinId()), &netres, webDavPassword.toUtf8().constData(),
                                 webDavUser.toUtf8().constData(), CONNECT_TEMPORARY | CONNECT_INTERACTIVE | CONNECT_REDIRECT,
                                 dst, &size, 0);
    if (r == NO_ERROR) {
        passStore = dst;
    } else {
        char message[256] = {0};
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, r, 0, message, sizeof(message), 0);
        ui->textBrowser->setTextColor(Qt::red);
        ui->textBrowser->setText(tr("Failed to connect WebDAV:\n") + message + " (0x" + QString::number(r, 16) + ")");
    }
#else
    fusedav.start("fusedav -o nonempty -u \"" + webDavUser + "\" " + webDavUrl + " \"" + passStore + '"');
    fusedav.waitForStarted();
    if (fusedav.state() == QProcess::Running) {
        QString pwd = webDavPassword;
        bool ok = true;
        if (pwd.isEmpty()) {
            pwd = QInputDialog::getText(this, tr("QtPass WebDAV password"),
               tr("Enter password to connect to WebDAV:"), QLineEdit::Password, "", &ok);
        }
        if (ok && !pwd.isEmpty()) {
            fusedav.write(pwd.toUtf8() + '\n');
            fusedav.closeWriteChannel();
            fusedav.waitForFinished(2000);
        } else {
            fusedav.terminate();
        }
    }
    QString error = fusedav.readAllStandardError();
    int prompt = error.indexOf("Password:");
    if (prompt >= 0) {
        error.remove(0, prompt + 10);
    }
    if (fusedav.state() != QProcess::Running) {
        error = tr("fusedav exited unexpectedly\n") + error;
    }
    if (error.size() > 0) {
        ui->textBrowser->setTextColor(Qt::red);
        ui->textBrowser->setText(tr("Failed to start fusedav to connect WebDAV:\n") + error);
    }
#endif
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

    useWebDav = (settings.value("useWebDav") == "true");
    webDavUrl = settings.value("webDavUrl").toString();
    webDavUser = settings.value("webDavUser").toString();
    webDavPassword = settings.value("webDavPassword").toString();

    if (passExecutable == "" && (gitExecutable == "" || gpgExecutable == "")) {
        config();
    }

    // TODO: this needs to be before we try to access the store,
    // but it would be better to do it after the Window is shown,
    // as the long delay it can cause is irritating otherwise.
    if (useWebDav) {
        mountWebDav();
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

QString MainWindow::getDir(const QModelIndex &index, bool forPass)
{
    if (!index.isValid()) {
        return forPass ? "" : passStore;
    }
    QFileInfo info = model.fileInfo(proxyModel.mapToSource(index));
    QString filePath = (info.isFile() ? info.absolutePath() : info.absoluteFilePath()) + '/';
    if (forPass) {
        filePath.replace(QRegExp("^" + passStore), "");
    }
    return filePath;
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
    ui->textBrowser->clear();
    ui->textBrowser->setTextColor(Qt::black);
    enableUiElements(false);
    process->start('"' + app + "\" " + args);
    if (!input.isEmpty()) {
        process->write(input.toUtf8());
    }
    process->closeWriteChannel();
}

/**
 * @brief MainWindow::readyRead
 */
void MainWindow::readyRead(bool finished = false) {
    QString output;
    if (currentAction != GPG_INTERNAL) {
    output = process->readAllStandardOutput();
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
    }

    QString error = process->readAllStandardError();
    if (error.size() > 0) {
        output = "<font color=\"red\">" + error + "</font><br />" + output;
    }

    output.replace(QRegExp("((http|https|ftp)\\://[a-zA-Z0-9\\-\\.]+\\.[a-zA-Z]{2,3}(:[a-zA-Z0-9]*)?/?([a-zA-Z0-9\\-\\._\\?\\,\\'/\\\\+&amp;%\\$#\\=~])*)"), "<a href=\"\\1\">\\1</a>");
    output.replace(QRegExp("\n"), "<br />");
    ui->textBrowser->setHtml(ui->textBrowser->toHtml() + output);
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
    ui->usersButton->setEnabled(state);
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

QString MainWindow::getRecipientString(QString for_file, QString separator)
{
    QDir gpgIdPath(QFileInfo(for_file.startsWith(passStore) ? for_file : passStore + for_file).absoluteDir());
    bool found = false;
    while (gpgIdPath.exists() && gpgIdPath.absolutePath().startsWith(passStore))
    {
        if (QFile(gpgIdPath.absoluteFilePath(".gpg-id")).exists()) {
            found = true;
            break;
        }
        if (!gpgIdPath.cdUp()) {
            break;
        }
    }
    QFile gpgId(found ? gpgIdPath.absoluteFilePath(".gpg-id") : passStore + ".gpg-id");
    if (!gpgId.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    QString recipients;
    while (!gpgId.atEnd()) {
        QString recipient(gpgId.readLine());
        recipient = recipient.trimmed();
        if (!recipient.isEmpty()) {
            recipients += separator + '"' + recipient + '"';
        }
    }
    return recipients;
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
        QString recipients = getRecipientString(file, " -r ");
        if (recipients.isEmpty()) {
            QMessageBox::critical(this, tr("Can not edit"),
                tr("Could not read encryption key to use, .gpg-id file missing or invalid."));
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
    file = getDir(ui->treeView->currentIndex(), usePass) + file;
    if (!usePass) {
        file += ".gpg";
    }
    lastDecrypt = "";
    setPassword(file, false);
    executeWrapper(gitExecutable, "add " + file);
//    executeWrapper(gitExecutable, "commit -a -m \"Adding " + file + "\"");
}

void MainWindow::on_deleteButton_clicked()
{
    QString file = getFile(ui->treeView->currentIndex(), usePass);
    if (QMessageBox::question(this, tr("Delete password?"),
        tr("Are you sure you want to delete %1?").arg(file),
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

QList<UserInfo> MainWindow::listKeys(QString keystring)
{
    QList<UserInfo> users;
    currentAction = GPG_INTERNAL;
    executeWrapper(gpgExecutable , "--no-tty --with-colons --list-keys " + keystring);
    process->waitForFinished(2000);
    if (process->exitStatus() != QProcess::NormalExit) {
        return users;
    }
    QString currentKey;
    QString currentName;
    bool saved = true;
    QStringList keys = QString(process->readAllStandardOutput()).split(QRegExp("[\r\n]"), QString::SkipEmptyParts);
    foreach (QString key, keys) {
        QStringList props = key.split(':');
        if (props.size() < 10) {
            continue;
        }
        if(props[0] == "pub") {
            saved = false;
            currentKey  = props[4];
            currentName = props[9];
        } else if (props[0] == "uid") {
            if (currentName.isEmpty()) {
                currentName = props[9];
            }
        } else if (props[0] == "sub" && !saved) {
            UserInfo i;
            i.name = currentName;
            i.key_id = currentKey;
            users.append(i);
            saved = true;
        }
    }
    return users;
}

void MainWindow::on_usersButton_clicked()
{
    QList<UserInfo> users = listKeys();
    if (users.size() == 0) {
        QMessageBox::critical(this, tr("Can not get key list"),
            tr("Unable to get list of available gpg keys"));
        return;
    }
    QList<UserInfo> selected_users;
    QString dir = getDir(ui->treeView->currentIndex(), false);
    QString recipients = getRecipientString(dir.isEmpty() ? "" : dir);
    if (!recipients.isEmpty()) {
        selected_users = listKeys(recipients);
    }
    foreach (const UserInfo &sel, selected_users) {
        for (QList<UserInfo>::iterator it = users.begin(); it != users.end(); ++it) {
            if (sel.key_id == it->key_id) it->enabled = true;
        }
    }
    UsersDialog d(this);
    d.setUsers(&users);
    if (!d.exec()) {
        d.setUsers(NULL);
        return;
    }
    d.setUsers(NULL);
    QFile gpgId(dir + ".gpg-id");
    if (!gpgId.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Cannot update"),
            tr("Failed to open .gpg-id for writing."));
        return;
    }
    foreach (const UserInfo &user, users) {
        if (user.enabled) {
            gpgId.write((user.key_id + "\n").toUtf8());
        }
    }
    gpgId.close();
}

/**
 * @brief MainWindow::setApp
 * @param app
 */
void MainWindow::setApp(SingleApplication *app)
{
#if SINGLE_APP
    connect(app, SIGNAL(messageAvailable(QString)), this, SLOT(messageAvailable(QString)));
#endif
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
