@rem set path=C:\ComfyUI\python_embeded;%path%
cd %~dp0
echo python.exe bmp_to_png.py %1 >> debuglog_py.txt
python.exe bmp_to_png.py %1 >> debuglog_py.txt 2>&1
