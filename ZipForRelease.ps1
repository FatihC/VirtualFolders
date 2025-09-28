$Project = (Get-ChildItem -Path ".vs\bin\Win32\Release" -Filter "*.dll" | Select-Object -First 1).BaseName
New-Item -Path Releases -ItemType Directory -Force
$x86Version = (Get-Command ".vs\bin\Win32\Release\$Project.dll").FileVersionInfo.ProductVersion
Compress-Archive -DestinationPath "Releases\$Project-$x86Version-x86.zip" -Path ".vs\bin\Win32\Release\*"
$x64Version = (Get-Command ".vs\bin\x64\Release\$Project.dll").FileVersionInfo.ProductVersion
Compress-Archive -DestinationPath "Releases\$Project-$x64Version-x64.zip" -Path ".vs\bin\x64\Release\*"
$x86Hash = (Get-FileHash "Releases\$Project-$x86Version-x86.zip" -Algorithm SHA256).Hash
$x64Hash = (Get-FileHash "Releases\$Project-$x64Version-x64.zip" -Algorithm SHA256).Hash
$Hashes =  "SHA-256 Hash ($Project-$x64Version-x64.zip): " + $x64Hash + "`nSHA-256 Hash ($Project-$x86Version-x86.zip): " + $x86Hash
Out-File -FilePath "Releases\Hashes.txt" -Append -InputObject $Hashes
