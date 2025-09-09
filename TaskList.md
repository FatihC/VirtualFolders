# Feature Plan

- [x] Wrap In Folder
- [x] Rename Folder
- [x] Rename Folder text popup
- [x] File renamed event
- [x] Acilista filename'de path olabiliyor. backupFilePath doluysa "extract filename"
- [x] Cpp dosyalarını ayır
- [x] Edited/Saved icon event handle
- [x] File opened event
- [x] File edited event allways calls Watcher::updateWatcherPanelUnconditional (Scintilla:modified removed)
- [x] Write json at close
- [ ] Add file into folder
- [ ] Add folder into folder
- [x] Move file into folder
- [x] Move file out of folder
	- [x] Bir üste giderken problem
	- [x] Bir alta giderken
	- [x] Inner folder outer folder'dan cikarilmali
- [x] NPPN_DOCORDERCHANGED (FAILED) -> Doc Order yerine buffer ve path ile ilerlenecek
- [x] Ortadan bir dosya silince tree'nin güncellenmesi
- [x] Ust menuden dosya acma
- [?] DOsya npp penceresinin üzerine bırakınca (bu normal document listte de çalışmıyor)
- [x] Büyük dosyalarla test (200KB'lık side dosyası ile hata vermedi.)
- [x] Folder moving
- [x] Folder deleting (Unwrap)
- [ ] Use std::list instead of vector
- [ ] Degisik temalarda insertion line
- [ ] Internationalization
	- [ ] NPPN_LANGCHANGED
- [ ] Context Menu
	- [ ] Move to View Kaldı
- [x] onBeforeFileClosed'da order docOrder ayari
- [ ] Dark Mode - Light Mode Change Event
- [ ] View's
- [ ] Session
- [ ] Plugin Menu's
- [ ] Template'in kodlarından kurtul
- [x] NPPN_READONLYCHANGED: no event like this. Check everytime an item selected
- [ ] No Document List. Just tabs
- [ ] 



# Bugs
- [x] File delete does not delete file. just removes from the tree
- [x] Opens some files some not: had to change static to inline
- [x] Üst menüden dosya açınca tree'de görünmüyor
- [x] Bir dosyayı notepad++'in üzerine getirip bırakınca tree'de tüm path gözüküyor. Restart edince düzeliyor.
- [ ] Bir dosya bir klasörün hemen üstüne taşınırken klasör highlihted ise klasörün sonuna ekliyor. HATA MI?
- [ ] dosya ismi uzun olunca otomatik sağa kaydırıyor
- [x] dosya isminde '\u0000' olusmasi
- [ ] ctrl+pagedown secili item'i degistirmedi
- [ ] Duplicate order - Add to the end
- [ ] empty vData on exit