# Feature Plan

- [x] Wrap In Folder
- [x] Rename Folder
- [x] Rename Folder text popup
- [x] File renamed event
- [x] Acilista filename'de path olabiliyor. backupFilePath doluysa "extract filename"
- [x] Cpp dosyalarını ayır
- [x] Edited/Saved icon event handle
- [ ] File opened event
- [*] File edited event allways calls Watcher::updateWatcherPanelUnconditional (Scintilla:modified removed)
- [ ] Write json at close
- [ ] Add file into folder
- [ ] Add folder into folder
- [x] Move file into folder
- [x] Move file out of folder
	- [x] Bir üste giderken problem
	- [x] Bir alta giderken
- [ ] NPPN_DOCORDERCHANGED 
- [x] Ortadan bir dosya silince tree'nin güncellenmesi
- [x] Ust menuden dosya acma
- [?] DOsya npp penceresinin üzerine bırakınca (bu normal document listte de çalışmıyor)
- [x] Büyük dosyalarla test (200KB'lık side dosyası ile hata vermedi.)
- [ ] Folder moving
- [ ] Folder deleting (Unwrap)



# Bugs
- [x] File delete does not delete file. just removes from the tree
- [x] Opens some files some not: had to change static to inline
- [x] Üst menüden dosya açınca tree'de görünmüyor
- [x] Bir dosyayı notepad++'in üzerine getirip bırakınca tree'de tüm path gözüküyor. Restart edince düzeliyor.
- [ ] Bir dosya bir klasörün hemen üstüne taşınırken klasör highlihted ise klasörün sonuna ekliyor. HATA MI?
- [ ] dosya ismi uzun olunca otomatik sağa kaydırıyor
- [ ] 