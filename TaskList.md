# Feature Plan

- [x] Wrap In Folder
- [x] Rename Folder
- [x] Rename Folder text popup
- [x] File renamed event
- [x] Acilista filename'de path olabiliyor. backupFilePath doluysa "extract filename"
- [x] Cpp dosyalarını ayır
- [ ] Edited/Saved icon event handle
- [ ] File opened event
- [ ] File edited event allways calls Watcher::updateWatcherPanelUnconditional
- [ ] Write json at close
- [ ] Add file into folder
- [ ] Add folder into folder
- [x] Move file into folder
- [ ] Move file out of folder
	- [ ] Bir üste giderken problem
	- [ ] Bir alta giderken
- [ ] NPPN_DOCORDERCHANGED 
- [ ] 



# Bugs
- [ ] File delete does not delete file. just removes from the tree
- [x] Opens some files some not: had to change static to inline
- [ ] Üst menüden dosya açınca tree'de görünmüyor
- [ ] Bir dosyayı notepad++'in üzerine getirip bırakınca tree'de tüm path gözüküyor. Restart edince düzeliyor.