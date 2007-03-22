ls -recurse -include 'win32' | rm -recurse
ls -recurse -include '*.user' | rm -recurse
if(test-path QuakeForge.suo) { rm QuakeForge.suo -force }
if(test-path QuakeForge.ncb) { rm QuakeForge.ncb }
ls -recurse -include 'win32' | rm -recurse
ls -recurse -include '*.user' | rm -recurse
if(test-path QuakeForge.suo) { rm QuakeForge.suo -force }
if(test-path QuakeForge.ncb) { rm QuakeForge.ncb }