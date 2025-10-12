@rem set path=C:\ComfyUI\python_embeded;%path%
cd %~dp0
echo python.exe png_to_bmp.py temp_img_res.png %1 >> debuglog_py.txt
python.exe png_to_bmp.py temp_img_res.png  >> debuglog_py.txt 2>&1
