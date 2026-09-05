$target = Join-Path $env:LOCALAPPDATA 'metasequoiaime\helpcodes'
New-Item -ItemType Directory -Path $target -Force | Out-Null
Copy-Item -Path ".\helpcodes\*" -Destination $target -Recurse -Force
