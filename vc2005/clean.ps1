ls -recurse -include 'win32','*.user' | rm -recurse
if(test-path QuakeForge.suo) { rm QuakeForge.suo -force }
if(test-path QuakeForge.ncb) { rm QuakeForge.ncb }