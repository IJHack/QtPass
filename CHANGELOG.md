# Change Log

## [Unreleased](https://github.com/IJHack/QtPass/tree/HEAD)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.1.6...HEAD)

**Implemented enhancements:**

- Copy button for each custom field [\#291](https://github.com/IJHack/QtPass/issues/291)
- Feature Request: Use primary selection instead of clipboard [\#280](https://github.com/IJHack/QtPass/issues/280)
- Add primary selection as clipboard option [\#281](https://github.com/IJHack/QtPass/pull/281) ([annejan](https://github.com/annejan))
- Feature: CTRL/CMD + Q closes the mainwindow \#258 [\#259](https://github.com/IJHack/QtPass/pull/259) ([YoshiMan](https://github.com/YoshiMan))
- Feature/testing moved sources to src added tests [\#257](https://github.com/IJHack/QtPass/pull/257) ([annejan](https://github.com/annejan))
- enabled drag and drop support for passwords and passwordfolders [\#245](https://github.com/IJHack/QtPass/pull/245) ([YoshiMan](https://github.com/YoshiMan))

**Fixed bugs:**

- Do not hide passwords and no generator [\#267](https://github.com/IJHack/QtPass/issues/267)
- Weird behavior when turning on git support \(auto push/pull\) with non-clean git dir [\#128](https://github.com/IJHack/QtPass/issues/128)

**Closed issues:**

- Git commit signing [\#303](https://github.com/IJHack/QtPass/issues/303)
- Measure unit-test code coverage [\#298](https://github.com/IJHack/QtPass/issues/298)
- Config dialog: Propose "Password behaviour" label change [\#294](https://github.com/IJHack/QtPass/issues/294)
- make install currently broken. [\#289](https://github.com/IJHack/QtPass/issues/289)
- There is no `git cp` [\#272](https://github.com/IJHack/QtPass/issues/272)
- pass is apparently switching out pwgen [\#264](https://github.com/IJHack/QtPass/issues/264)
- pass working fine but qtprocess failure with qtpass [\#260](https://github.com/IJHack/QtPass/issues/260)
- Feature: CTRL/CMD + Q closes the mainwindow [\#258](https://github.com/IJHack/QtPass/issues/258)
- Refactoring: removal of lastDecrypt [\#256](https://github.com/IJHack/QtPass/issues/256)
- Pass environment not set-up correctly [\#250](https://github.com/IJHack/QtPass/issues/250)
- Make fails - std c++11 not set [\#244](https://github.com/IJHack/QtPass/issues/244)
- Double-clicking might open previous entry instead of one double-clicked on [\#243](https://github.com/IJHack/QtPass/issues/243)

**Merged pull requests:**

- Once again, code coverage [\#305](https://github.com/IJHack/QtPass/pull/305) ([tezeb](https://github.com/tezeb))
- Fixed path of resources.qrc [\#297](https://github.com/IJHack/QtPass/pull/297) ([sideeffect42](https://github.com/sideeffect42))
- Add pt\_PT translation [\#295](https://github.com/IJHack/QtPass/pull/295) ([keitalbame](https://github.com/keitalbame))
- Update README.md [\#293](https://github.com/IJHack/QtPass/pull/293) ([joostruis](https://github.com/joostruis))
- small band aid fix for password generation on windows [\#276](https://github.com/IJHack/QtPass/pull/276) ([treat1](https://github.com/treat1))
- Final step in process mgmt refactoring [\#275](https://github.com/IJHack/QtPass/pull/275) ([tezeb](https://github.com/tezeb))
- Fix pwgen and refactor Pass::finished [\#271](https://github.com/IJHack/QtPass/pull/271) ([tezeb](https://github.com/tezeb))
- Process specific signals for process management [\#270](https://github.com/IJHack/QtPass/pull/270) ([tezeb](https://github.com/tezeb))
- \#239 reencrypting after a drag and drop action [\#261](https://github.com/IJHack/QtPass/pull/261) ([YoshiMan](https://github.com/YoshiMan))
- this if evaluetes ervery time to true [\#255](https://github.com/IJHack/QtPass/pull/255) ([YoshiMan](https://github.com/YoshiMan))
- executeing pass show before editpassword dialog shows up [\#254](https://github.com/IJHack/QtPass/pull/254) ([YoshiMan](https://github.com/YoshiMan))
- Minor fix for file names and git push [\#251](https://github.com/IJHack/QtPass/pull/251) ([tezeb](https://github.com/tezeb))
- Process management refactoring part 2 [\#249](https://github.com/IJHack/QtPass/pull/249) ([tezeb](https://github.com/tezeb))

## [v1.1.6](https://github.com/IJHack/QtPass/tree/v1.1.6) (2016-12-02)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.1.5...v1.1.6)

**Implemented enhancements:**

- Feedback on copy button use [\#229](https://github.com/IJHack/QtPass/issues/229)
- Clickable url's + open in default browser [\#226](https://github.com/IJHack/QtPass/issues/226)
- Deselecting password re-opens the file  [\#221](https://github.com/IJHack/QtPass/issues/221)
- Copy password button should include tooltip to say why, when disabled [\#214](https://github.com/IJHack/QtPass/issues/214)
- QtPass starts by searching for -psn\_0\_12345 on macOS [\#213](https://github.com/IJHack/QtPass/issues/213)
- Copy after timeout [\#189](https://github.com/IJHack/QtPass/issues/189)
- Feature Request: Copy template fields with button [\#133](https://github.com/IJHack/QtPass/issues/133)
- Cannot create top level folder [\#127](https://github.com/IJHack/QtPass/issues/127)
- Feature: moving items \(reordering folders\) [\#116](https://github.com/IJHack/QtPass/issues/116)
- Password dialog decoupling from MW [\#242](https://github.com/IJHack/QtPass/pull/242) ([tezeb](https://github.com/tezeb))
- Refactoring of qpushbuttonwithclipboard and timers [\#241](https://github.com/IJHack/QtPass/pull/241) ([tezeb](https://github.com/tezeb))
- added a copy button for each line to paste the content into the clipboard, "pass init -- path=" command with right path-parameter, lupdate qtpass.pro [\#218](https://github.com/IJHack/QtPass/pull/218) ([YoshiMan](https://github.com/YoshiMan))

**Fixed bugs:**

- Regression with new view mode when using templates and urls [\#223](https://github.com/IJHack/QtPass/issues/223)
- Problems with high dpi screen [\#217](https://github.com/IJHack/QtPass/issues/217)
- Hangs forever on Generate GnuPG keypair [\#215](https://github.com/IJHack/QtPass/issues/215)
- Copy after timeout [\#189](https://github.com/IJHack/QtPass/issues/189)
- recent change to passworddialog.cpp [\#188](https://github.com/IJHack/QtPass/issues/188)
- Re-opening entry in QtPass on Windows does not put login or url values back in the right place [\#183](https://github.com/IJHack/QtPass/issues/183)

**Closed issues:**

- Click does not stick [\#233](https://github.com/IJHack/QtPass/issues/233)
- Doubleclick on Treeview does not open the edit dialouge [\#228](https://github.com/IJHack/QtPass/issues/228)
- Windows - Enable GPG SSH Authentication [\#225](https://github.com/IJHack/QtPass/issues/225)
- We need autotype . .  [\#65](https://github.com/IJHack/QtPass/issues/65)

**Merged pull requests:**

- refactoring - pass ifce, process mgmt [\#234](https://github.com/IJHack/QtPass/pull/234) ([tezeb](https://github.com/tezeb))
- Solve Doubleclick issue  [\#230](https://github.com/IJHack/QtPass/pull/230) ([jounathaen](https://github.com/jounathaen))
- refactoring, new QtPassSettings class, all settings should be read and written here [\#224](https://github.com/IJHack/QtPass/pull/224) ([YoshiMan](https://github.com/YoshiMan))
- Moved @YoshiMan 's copy buttons inside the line Edit [\#222](https://github.com/IJHack/QtPass/pull/222) ([jounathaen](https://github.com/jounathaen))
- UI Improvements [\#220](https://github.com/IJHack/QtPass/pull/220) ([jounathaen](https://github.com/jounathaen))
- creating password store directory, if it doesnot exists, de\_DE translation fixes and removed obsolete translations [\#216](https://github.com/IJHack/QtPass/pull/216) ([YoshiMan](https://github.com/YoshiMan))

## [v1.1.5](https://github.com/IJHack/QtPass/tree/v1.1.5) (2016-10-19)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.1.4...v1.1.5)

**Implemented enhancements:**

- I translated for Simplified Chinese.  [\#208](https://github.com/IJHack/QtPass/issues/208)
- Short fullname hangs QtPass keypair generation process for infinite time [\#202](https://github.com/IJHack/QtPass/issues/202)
- More options for password generation [\#98](https://github.com/IJHack/QtPass/issues/98)
- Git hangs on windows [\#71](https://github.com/IJHack/QtPass/issues/71)

**Fixed bugs:**

- view box is trimming whitespace [\#210](https://github.com/IJHack/QtPass/issues/210)
- Short fullname hangs QtPass keypair generation process for infinite time [\#202](https://github.com/IJHack/QtPass/issues/202)

**Closed issues:**

- PREFIX is now really a prefix [\#185](https://github.com/IJHack/QtPass/issues/185)
- QtPass, git and windows [\#173](https://github.com/IJHack/QtPass/issues/173)

**Merged pull requests:**

- Allow ssh links [\#211](https://github.com/IJHack/QtPass/pull/211) ([cgonzalez](https://github.com/cgonzalez))
- Increase maximum password length to 255 [\#209](https://github.com/IJHack/QtPass/pull/209) ([vladimiroff](https://github.com/vladimiroff))
- Password templates [\#207](https://github.com/IJHack/QtPass/pull/207) ([jounathaen](https://github.com/jounathaen))
- Updated German Translation [\#206](https://github.com/IJHack/QtPass/pull/206) ([jounathaen](https://github.com/jounathaen))
- Italian translation [\#204](https://github.com/IJHack/QtPass/pull/204) ([dakk](https://github.com/dakk))
- keygendialog email and name validition \(issue 202\) [\#203](https://github.com/IJHack/QtPass/pull/203) ([dakk](https://github.com/dakk))
- Lookup validity field to check if keys are valid [\#201](https://github.com/IJHack/QtPass/pull/201) ([thotypous](https://github.com/thotypous))
- Fix spelling error [\#200](https://github.com/IJHack/QtPass/pull/200) ([innir](https://github.com/innir))

## [v1.1.4](https://github.com/IJHack/QtPass/tree/v1.1.4) (2016-09-26)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.1.3...v1.1.4)

**Implemented enhancements:**

- Re-assign permissions when adding users [\#161](https://github.com/IJHack/QtPass/issues/161)
- Main window immediately closes upon app launch [\#139](https://github.com/IJHack/QtPass/issues/139)

**Fixed bugs:**

- German umlauts fails [\#192](https://github.com/IJHack/QtPass/issues/192)
- Error after change configuration [\#190](https://github.com/IJHack/QtPass/issues/190)
- Re-assign permissions when adding users [\#161](https://github.com/IJHack/QtPass/issues/161)
- Bug: Special characters in Template [\#131](https://github.com/IJHack/QtPass/issues/131)
- Character encoding issue with GPG key [\#101](https://github.com/IJHack/QtPass/issues/101)
- saved password '§' turns to 'Â§' when copied to clipboard or shown when editing [\#91](https://github.com/IJHack/QtPass/issues/91)

**Closed issues:**

- Signed releases [\#186](https://github.com/IJHack/QtPass/issues/186)
- Why it's not listed in wikipedia.org/wiki/List\_of\_password\_managers ? [\#164](https://github.com/IJHack/QtPass/issues/164)
- Bitdefender blocks installation and quarantines the .exe and .ink [\#138](https://github.com/IJHack/QtPass/issues/138)

**Merged pull requests:**

- issue 91 bugfix [\#199](https://github.com/IJHack/QtPass/pull/199) ([asalamon74](https://github.com/asalamon74))
- issue 101 bugfix [\#198](https://github.com/IJHack/QtPass/pull/198) ([asalamon74](https://github.com/asalamon74))
- ArchLinux: moved from AUR to \[community\] [\#196](https://github.com/IJHack/QtPass/pull/196) ([eworm-de](https://github.com/eworm-de))
- Czech translation [\#195](https://github.com/IJHack/QtPass/pull/195) ([svetlemodry](https://github.com/svetlemodry))

## [v1.1.3](https://github.com/IJHack/QtPass/tree/v1.1.3) (2016-06-10)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.1.2...v1.1.3)

**Fixed bugs:**

- edit of password broken with active "Automatically push" [\#177](https://github.com/IJHack/QtPass/issues/177)
- Clipboard not cleared when quitting or killing application [\#171](https://github.com/IJHack/QtPass/issues/171)
- Hide content doesn't work when using templates [\#160](https://github.com/IJHack/QtPass/issues/160)

**Closed issues:**

- Add a \(small\) manpage [\#174](https://github.com/IJHack/QtPass/issues/174)

## [v1.1.2](https://github.com/IJHack/QtPass/tree/v1.1.2) (2016-06-10)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.1.1...v1.1.2)

**Implemented enhancements:**

- qtpass on windows, space in front of URL and Username [\#182](https://github.com/IJHack/QtPass/issues/182)

**Fixed bugs:**

- Deletion of folder doesn't work on Debian/GNU Linux [\#181](https://github.com/IJHack/QtPass/issues/181)

**Closed issues:**

- gpg: decryption failed: No secret key [\#179](https://github.com/IJHack/QtPass/issues/179)
- "gpg-agent: command get\_passphrase failed: No such file or directory" [\#156](https://github.com/IJHack/QtPass/issues/156)

**Merged pull requests:**

- add Appdata file and update desktop file [\#178](https://github.com/IJHack/QtPass/pull/178) ([daveol](https://github.com/daveol))
- HTTPS everywhere [\#176](https://github.com/IJHack/QtPass/pull/176) ([da2x](https://github.com/da2x))
- Fix build issues with MSVC2015 on Windows [\#175](https://github.com/IJHack/QtPass/pull/175) ([msvi](https://github.com/msvi))

## [v1.1.1](https://github.com/IJHack/QtPass/tree/v1.1.1) (2016-04-04)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.1.0...v1.1.1)

**Implemented enhancements:**

- Signed binaries [\#149](https://github.com/IJHack/QtPass/issues/149)
- Icon theme and Cinnamon [\#146](https://github.com/IJHack/QtPass/issues/146)
- Bind a key to the clear action [\#142](https://github.com/IJHack/QtPass/issues/142)
- Installation dependencies [\#140](https://github.com/IJHack/QtPass/issues/140)
- All text input fields need example text & edit dialogue changes [\#85](https://github.com/IJHack/QtPass/issues/85)
- OSX: Qt-window closed only reappears when 'active' and using tray incon [\#77](https://github.com/IJHack/QtPass/issues/77)

**Fixed bugs:**

- Program does not run in WIndows 10 [\#123](https://github.com/IJHack/QtPass/issues/123)
- Spelling bug: German translation of push and pull [\#110](https://github.com/IJHack/QtPass/issues/110)
- gpg: decryption failed: No secret key [\#92](https://github.com/IJHack/QtPass/issues/92)

**Closed issues:**

- Remove outdated Debian packaging [\#165](https://github.com/IJHack/QtPass/issues/165)
- Same name for file and folder [\#159](https://github.com/IJHack/QtPass/issues/159)
- Icons don't work on nixos [\#157](https://github.com/IJHack/QtPass/issues/157)
- gpg: Sorry, we are in batchmode - can't get input [\#151](https://github.com/IJHack/QtPass/issues/151)

**Merged pull requests:**

- lupdate and Russian translation [\#170](https://github.com/IJHack/QtPass/pull/170) ([ahippo](https://github.com/ahippo))
- Remove path to password store in commit message and a leading space. [\#169](https://github.com/IJHack/QtPass/pull/169) ([ahippo](https://github.com/ahippo))
- Use --secure for pwgen and add more configurable options [\#168](https://github.com/IJHack/QtPass/pull/168) ([ahippo](https://github.com/ahippo))
- Remove Debian packaging [\#166](https://github.com/IJHack/QtPass/pull/166) ([innir](https://github.com/innir))
- Add caskroom URL [\#163](https://github.com/IJHack/QtPass/pull/163) ([graingert](https://github.com/graingert))
- update gl\_Es [\#162](https://github.com/IJHack/QtPass/pull/162) ([xmgz](https://github.com/xmgz))
- Two UI Tweaks [\#158](https://github.com/IJHack/QtPass/pull/158) ([lftl](https://github.com/lftl))
- configwindow.ui default/start tab set to "settings" [\#154](https://github.com/IJHack/QtPass/pull/154) ([jounathaen](https://github.com/jounathaen))
- FAQ update concerning button-icons on cinnamon [\#153](https://github.com/IJHack/QtPass/pull/153) ([jounathaen](https://github.com/jounathaen))

## [v1.1.0](https://github.com/IJHack/QtPass/tree/v1.1.0) (2016-01-25)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.0.6...v1.1.0)

**Implemented enhancements:**

- Clear text input: use system icon instead of x [\#84](https://github.com/IJHack/QtPass/issues/84)

**Closed issues:**

- \[resolved\] Error in compiling Mac OS El capitan [\#148](https://github.com/IJHack/QtPass/issues/148)

**Merged pull requests:**

- Pre 1.1 mixing [\#145](https://github.com/IJHack/QtPass/pull/145) ([annejan](https://github.com/annejan))
- Futurator Keygen [\#144](https://github.com/IJHack/QtPass/pull/144) ([annejan](https://github.com/annejan))
- Futurator redesign proper [\#141](https://github.com/IJHack/QtPass/pull/141) ([annejan](https://github.com/annejan))
- RPM Spec file updates [\#137](https://github.com/IJHack/QtPass/pull/137) ([muff1nman](https://github.com/muff1nman))
- swedish translations [\#135](https://github.com/IJHack/QtPass/pull/135) ([ralphtheninja](https://github.com/ralphtheninja))

## [v1.0.6](https://github.com/IJHack/QtPass/tree/v1.0.6) (2016-01-03)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.0.5...v1.0.6)

**Implemented enhancements:**

- Feature: Always on top [\#118](https://github.com/IJHack/QtPass/issues/118)
- Option to show minimized instance [\#99](https://github.com/IJHack/QtPass/issues/99)
- System Icons on Buttons and Doubleclick on treeView [\#124](https://github.com/IJHack/QtPass/pull/124) ([jounathaen](https://github.com/jounathaen))

**Fixed bugs:**

- Bug: deleted record stays in memory [\#117](https://github.com/IJHack/QtPass/issues/117)

**Closed issues:**

- SIGSEGV in MainWindow::executeWrapper on clean install [\#122](https://github.com/IJHack/QtPass/issues/122)

**Merged pull requests:**

- improved the German translation [\#134](https://github.com/IJHack/QtPass/pull/134) ([retokromer](https://github.com/retokromer))
- qrand always generating the same sequence of passwords [\#129](https://github.com/IJHack/QtPass/pull/129) ([treat1](https://github.com/treat1))
- some improvements [\#126](https://github.com/IJHack/QtPass/pull/126) ([retokromer](https://github.com/retokromer))
- added one translation [\#125](https://github.com/IJHack/QtPass/pull/125) ([retokromer](https://github.com/retokromer))
- initial attempt to create a RPM spec file [\#121](https://github.com/IJHack/QtPass/pull/121) ([bram-ivs](https://github.com/bram-ivs))
- Cleanup and coding standards [\#120](https://github.com/IJHack/QtPass/pull/120) ([annejan](https://github.com/annejan))
- Modified the clipboard logic to allow for on-demand copy to clipboard. [\#119](https://github.com/IJHack/QtPass/pull/119) ([jonhanks](https://github.com/jonhanks))

## [v1.0.5](https://github.com/IJHack/QtPass/tree/v1.0.5) (2015-11-18)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.0.4...v1.0.5)

**Fixed bugs:**

- using pwgen adds carriage-return [\#115](https://github.com/IJHack/QtPass/issues/115)
- Enhancement: color code git results [\#111](https://github.com/IJHack/QtPass/issues/111)

**Merged pull requests:**

- Fix bug that prints "Unknown error" to the terminal [\#113](https://github.com/IJHack/QtPass/pull/113) ([dvarum12](https://github.com/dvarum12))

## [v1.0.4](https://github.com/IJHack/QtPass/tree/v1.0.4) (2015-11-03)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.0.3...v1.0.4)

**Implemented enhancements:**

- Add support for RightToLeft languages [\#108](https://github.com/IJHack/QtPass/issues/108)

## [v1.0.3](https://github.com/IJHack/QtPass/tree/v1.0.3) (2015-10-25)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.0.2...v1.0.3)

**Implemented enhancements:**

- Get PREFIX variable from environment [\#106](https://github.com/IJHack/QtPass/issues/106)
- Password file named 'git' returns error [\#105](https://github.com/IJHack/QtPass/issues/105)

**Fixed bugs:**

- Password file named 'git' returns error [\#105](https://github.com/IJHack/QtPass/issues/105)

**Merged pull requests:**

- Get PREFIX variable from environment [\#104](https://github.com/IJHack/QtPass/pull/104) ([jorti](https://github.com/jorti))
- spanish translations added [\#103](https://github.com/IJHack/QtPass/pull/103) ([mrpnkt](https://github.com/mrpnkt))

## [v1.0.2](https://github.com/IJHack/QtPass/tree/v1.0.2) (2015-09-24)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.0.1...v1.0.2)

**Closed issues:**

- Generate password: Floating point exception \(core dumped\) [\#102](https://github.com/IJHack/QtPass/issues/102)
- A way to indicate the installation prefix is needed [\#100](https://github.com/IJHack/QtPass/issues/100)
- IPv4 URLs are non-clickable [\#97](https://github.com/IJHack/QtPass/issues/97)
- app crashes when "Use pwgen" is unselected, and "Generate" is clicked. [\#95](https://github.com/IJHack/QtPass/issues/95)
- Some minor improvements on the templating part [\#93](https://github.com/IJHack/QtPass/issues/93)
- app crashes with variant of "pwgen" app [\#90](https://github.com/IJHack/QtPass/issues/90)

## [v1.0.1](https://github.com/IJHack/QtPass/tree/v1.0.1) (2015-08-09)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.0.0...v1.0.1)

**Implemented enhancements:**

- Users setup - key colours could be improved  [\#82](https://github.com/IJHack/QtPass/issues/82)

**Closed issues:**

- When QtPass starts, focus search input box [\#89](https://github.com/IJHack/QtPass/issues/89)
- Clear the password display after some time [\#86](https://github.com/IJHack/QtPass/issues/86)
- Auto push/pull [\#83](https://github.com/IJHack/QtPass/issues/83)
- qtpass doesn't commit deletes to git [\#81](https://github.com/IJHack/QtPass/issues/81)
- Always crashes while using the quick-search input [\#79](https://github.com/IJHack/QtPass/issues/79)
- Git initialisation [\#72](https://github.com/IJHack/QtPass/issues/72)
- Initialising new repo's doesn't work correctly [\#55](https://github.com/IJHack/QtPass/issues/55)
- gpg: Sorry, no terminal at all requested - can't get input [\#18](https://github.com/IJHack/QtPass/issues/18)

**Merged pull requests:**

- Issue 86 clear panel [\#87](https://github.com/IJHack/QtPass/pull/87) ([karlgrz](https://github.com/karlgrz))
- Update FAQ for Yubikey NEO helper in .bashrc for Ubuntu [\#80](https://github.com/IJHack/QtPass/pull/80) ([karlgrz](https://github.com/karlgrz))
- \[WIP\] Call 'pass git init' on creation of password-store when useGit [\#78](https://github.com/IJHack/QtPass/pull/78) ([dennisdegreef](https://github.com/dennisdegreef))

## [v1.0.0](https://github.com/IJHack/QtPass/tree/v1.0.0) (2015-08-01)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v0.9.2...v1.0.0)

**Closed issues:**

- Yubikey Neo Pin entry not working properly on Ubuntu 15.04 [\#73](https://github.com/IJHack/QtPass/issues/73)

**Merged pull requests:**

- Updating hungarian localisation [\#76](https://github.com/IJHack/QtPass/pull/76) ([damnlie](https://github.com/damnlie))
- added DE translations [\#74](https://github.com/IJHack/QtPass/pull/74) ([Friedy](https://github.com/Friedy))

## [v0.9.2](https://github.com/IJHack/QtPass/tree/v0.9.2) (2015-07-30)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v0.9.1...v0.9.2)

**Closed issues:**

- Show expiration date in key setup [\#70](https://github.com/IJHack/QtPass/issues/70)

## [v0.9.1](https://github.com/IJHack/QtPass/tree/v0.9.1) (2015-07-29)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v0.9.0...v0.9.1)

**Closed issues:**

- Minimize on startup. [\#69](https://github.com/IJHack/QtPass/issues/69)
- tray icon in xfce [\#58](https://github.com/IJHack/QtPass/issues/58)
- Git intergration [\#57](https://github.com/IJHack/QtPass/issues/57)
- Weird characters in filenames breaks loading gpg files [\#10](https://github.com/IJHack/QtPass/issues/10)

## [v0.9.0](https://github.com/IJHack/QtPass/tree/v0.9.0) (2015-07-17)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v0.8.6...v0.9.0)

**Closed issues:**

- Request:  Integrate qtpass with pwgen for generating passwords.  [\#68](https://github.com/IJHack/QtPass/issues/68)

## [v0.8.6](https://github.com/IJHack/QtPass/tree/v0.8.6) (2015-07-17)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v0.8.5.1...v0.8.6)

**Closed issues:**

- Copy password by Ctrl+C [\#60](https://github.com/IJHack/QtPass/issues/60)
- Remember window size and vertical pane width [\#59](https://github.com/IJHack/QtPass/issues/59)
- Multiline Editing [\#34](https://github.com/IJHack/QtPass/issues/34)

**Merged pull requests:**

- To make building successfull wi Desktop Qt 5.4.0 MSVC2012 OpenGL 32bit [\#67](https://github.com/IJHack/QtPass/pull/67) ([annejan](https://github.com/annejan))

## [v0.8.5.1](https://github.com/IJHack/QtPass/tree/v0.8.5.1) (2015-07-08)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v0.8.5...v0.8.5.1)

## [v0.8.5](https://github.com/IJHack/QtPass/tree/v0.8.5) (2015-07-08)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v0.8.4...v0.8.5)

**Closed issues:**

- Won't compile on Kubuntu 15.10 [\#61](https://github.com/IJHack/QtPass/issues/61)
- Hanging process gives weird effects [\#56](https://github.com/IJHack/QtPass/issues/56)
- Directory separator actually broken by 208171fd09c55ad765fdf4fa1de9a7f0757fa72d [\#53](https://github.com/IJHack/QtPass/issues/53)

**Merged pull requests:**

- Many deadlocks and other nasty bug fixes [\#64](https://github.com/IJHack/QtPass/pull/64) ([annejan](https://github.com/annejan))
- Mention qt5-default package in README [\#62](https://github.com/IJHack/QtPass/pull/62) ([lorrin](https://github.com/lorrin))
- Some hacks I needed for portable gpg4win release [\#54](https://github.com/IJHack/QtPass/pull/54) ([rdoeffinger](https://github.com/rdoeffinger))

## [v0.8.4](https://github.com/IJHack/QtPass/tree/v0.8.4) (2015-06-11)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v0.8.3...v0.8.4)

**Closed issues:**

- QtPass does not detect GPG installation [\#50](https://github.com/IJHack/QtPass/issues/50)
- Cannot create new folders [\#48](https://github.com/IJHack/QtPass/issues/48)
- Better error handling when no pass or gpg found initially [\#13](https://github.com/IJHack/QtPass/issues/13)

**Merged pull requests:**

- Develop [\#52](https://github.com/IJHack/QtPass/pull/52) ([annejan](https://github.com/annejan))
- Minor thingies [\#51](https://github.com/IJHack/QtPass/pull/51) ([beefcurtains](https://github.com/beefcurtains))

## [v0.8.3](https://github.com/IJHack/QtPass/tree/v0.8.3) (2015-06-09)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v0.8.2...v0.8.3)

**Merged pull requests:**

- Bugfixes [\#49](https://github.com/IJHack/QtPass/pull/49) ([rdoeffinger](https://github.com/rdoeffinger))

## [v0.8.2](https://github.com/IJHack/QtPass/tree/v0.8.2) (2015-05-27)
[Full Changelog](https://github.com/IJHack/QtPass/compare/v0.8.1...v0.8.2)

**Closed issues:**

- Typo in 37f17f3808c1c97bd72c165a530c67a4bfb82edb? [\#45](https://github.com/IJHack/QtPass/issues/45)
- Signing of keys from user management [\#41](https://github.com/IJHack/QtPass/issues/41)

**Merged pull requests:**

- Provide more information in user list. [\#47](https://github.com/IJHack/QtPass/pull/47) ([rdoeffinger](https://github.com/rdoeffinger))
- Enable C++11 and use it to simplify loops. [\#46](https://github.com/IJHack/QtPass/pull/46) ([rdoeffinger](https://github.com/rdoeffinger))

## [v0.8.1](https://github.com/IJHack/QtPass/tree/v0.8.1) (2015-05-06)
**Fixed bugs:**

- Some items not found on first search [\#8](https://github.com/IJHack/QtPass/issues/8)

**Closed issues:**

- compiling qtpass on ubuntu 15.04 - fails due to newer qmake version [\#43](https://github.com/IJHack/QtPass/issues/43)
- QProcess::start: Process is already running [\#40](https://github.com/IJHack/QtPass/issues/40)
- Extra line breaks seem to be added to the \(html\) output [\#39](https://github.com/IJHack/QtPass/issues/39)
- Missing develop branch and release testing [\#38](https://github.com/IJHack/QtPass/issues/38)
- Windows WebDAV broken by 24f8dec3c203921f765e923e6ae6a4069b8cf50a [\#36](https://github.com/IJHack/QtPass/issues/36)
- .gpg-id file not added to git [\#35](https://github.com/IJHack/QtPass/issues/35)
- Icon filenames [\#31](https://github.com/IJHack/QtPass/issues/31)
- `GNUPGHOME` environment variable [\#30](https://github.com/IJHack/QtPass/issues/30)
- Feature: webdav alternative to git [\#28](https://github.com/IJHack/QtPass/issues/28)
- Windows: not working due to pointless use of "sh" [\#16](https://github.com/IJHack/QtPass/issues/16)
- Windows: support static build and enable ASLR and NX [\#15](https://github.com/IJHack/QtPass/issues/15)
- Some paths to executables are printed when starting up [\#11](https://github.com/IJHack/QtPass/issues/11)

**Merged pull requests:**

- SingleApplication per user and leading newline removed from output [\#44](https://github.com/IJHack/QtPass/pull/44) ([annejan](https://github.com/annejan))
- User filtering and many fixes [\#42](https://github.com/IJHack/QtPass/pull/42) ([annejan](https://github.com/annejan))
- Re-enable Windows WebDAV support. [\#37](https://github.com/IJHack/QtPass/pull/37) ([rdoeffinger](https://github.com/rdoeffinger))
- User robustness [\#33](https://github.com/IJHack/QtPass/pull/33) ([rdoeffinger](https://github.com/rdoeffinger))
- Add WebDAV support. [\#29](https://github.com/IJHack/QtPass/pull/29) ([rdoeffinger](https://github.com/rdoeffinger))
- Add nosingleapp config. [\#27](https://github.com/IJHack/QtPass/pull/27) ([rdoeffinger](https://github.com/rdoeffinger))
- Add Makefile with commands to make a binary release zip file. [\#25](https://github.com/IJHack/QtPass/pull/25) ([rdoeffinger](https://github.com/rdoeffinger))
- Start process only after we finished disabling UI elements etc. [\#24](https://github.com/IJHack/QtPass/pull/24) ([rdoeffinger](https://github.com/rdoeffinger))
- Support for editing .gpg-id via GUI with public keyring list. [\#23](https://github.com/IJHack/QtPass/pull/23) ([rdoeffinger](https://github.com/rdoeffinger))
- More proper support for subdirectories. [\#22](https://github.com/IJHack/QtPass/pull/22) ([rdoeffinger](https://github.com/rdoeffinger))
- Russian translation \(+typo fixed\) [\#20](https://github.com/IJHack/QtPass/pull/20) ([mexus](https://github.com/mexus))
- Windows-related fixes. [\#17](https://github.com/IJHack/QtPass/pull/17) ([rdoeffinger](https://github.com/rdoeffinger))
- Deal with "special" characters [\#14](https://github.com/IJHack/QtPass/pull/14) ([JiCiT](https://github.com/JiCiT))
- galician and spanish localization files created [\#12](https://github.com/IJHack/QtPass/pull/12) ([xmgz](https://github.com/xmgz))
- Update localization\_hu\_HU.ts [\#9](https://github.com/IJHack/QtPass/pull/9) ([damnlie](https://github.com/damnlie))
- Replace which invocations with actual path resolution code [\#7](https://github.com/IJHack/QtPass/pull/7) ([shitbangs](https://github.com/shitbangs))
- Added Swedish and Polish localization to resources [\#6](https://github.com/IJHack/QtPass/pull/6) ([iamtew](https://github.com/iamtew))
- Swedish localization [\#5](https://github.com/IJHack/QtPass/pull/5) ([iamtew](https://github.com/iamtew))
- Update localization\_hu\_HU.ts [\#4](https://github.com/IJHack/QtPass/pull/4) ([reesenemesis](https://github.com/reesenemesis))
- Update localization\_hu\_HU.ts [\#3](https://github.com/IJHack/QtPass/pull/3) ([reesenemesis](https://github.com/reesenemesis))
- \[pass\]\(http://www.passwordstore.org/\) [\#2](https://github.com/IJHack/QtPass/pull/2) ([guaka](https://github.com/guaka))
- Beginning of German translation [\#1](https://github.com/IJHack/QtPass/pull/1) ([mwfc](https://github.com/mwfc))



\* *This Change Log was automatically generated by [github_changelog_generator](https://github.com/skywinder/Github-Changelog-Generator)*