# Version 1.0.3
- [] When dropped a file into a folder new selection not working
- [x] Put localization folder into project but not in dll


# Version 1.0.2
- [x] When NPP is closed, if you open a file with NPP file does not show up on the tree.
- [?] Renamed buffer icon stays saved (Could not reproduce)
- [x] At startup current buffer is not selected in the tree
- [x] If a file in session.xml has turkish characters it corrupts the tree


# Feature Plan
- [ ] A new menu for option to use OS file icons
- [ ] Use OS file icons on the tree

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
- [x] Use std::list instead of vector: vector is recommended
- [x] When Theme changed
	- [x] Degisik temalarda insertion line: use text color as lineColor 
- [x] Internationalization
	- [x] NPPN_LANGCHANGED
- [x] Context Menu
	- [x] Move to View Kaldı
- [x] onBeforeFileClosed'da order docOrder ayari
- [x] Dark Mode - Light Mode Change Event
- [x] View's
- [x] Session
- [x] Plugin Menu's
- [x] Template'in kodlarından kurtul
- [x] NPPN_READONLYCHANGED: no event like this. Check everytime an item selected
- [x] No Document List. Just tabs
- [x] new buffer uretildi ama restart olmadi. path'de sadece new 1 yaziyor. 
	- [x] restart olmuyor json'i silmezsen
- [x] File deleted from disk
	- [x] While notepad++ is running
	- [x] While notepad++ is NOT running
- [x] NPPN_FILEDELETEFAILED: gerek yok
- [x] If there is a problem with orders. fix at the start. don't crash
- [-] Hover color in different themes: seems not possible. In dark theme my color is ignored.
- [x] ctrl+pg dwn-up: catch these events. and switch to new buffer with order number not docOrder
- [x] Font resize.



# Bugs
- [x] File delete does not delete file. just removes from the tree
- [x] Opens some files some not: had to change static to inline
- [x] Üst menüden dosya açınca tree'de görünmüyor
- [x] Bir dosyayı notepad++'in üzerine getirip bırakınca tree'de tüm path gözüküyor. Restart edince düzeliyor.
- [x] Bir dosya bir klasörün hemen üstüne taşınırken klasör highlihted ise klasörün sonuna ekliyor. HATA MI?
- [x] dosya ismi uzun olunca otomatik sağa kaydırıyor
- [x] dosya isminde '\u0000' olusmasi
- [x] ctrl+pagedown secili item'i degistirmedi
- [x] Duplicate order - Add to the end
- [x] empty vData on exit
- [x] Acilista view=1 olan ama subview'de olmayanlari view=0 yap
- [ ] 