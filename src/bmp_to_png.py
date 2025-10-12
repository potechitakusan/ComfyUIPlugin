from PIL import Image
import sys
from pathlib import Path

def convert_bmp_to_png(input_path: str, output_path: str = None):
    # BMP ファイルを PNG に変換
    input_file = Path(input_path)
    if not input_file.exists():
        print(f"入力ファイルが見つかりません: {input_file}")
        return

    # 出力先を指定しなければ、同じ場所に同名の .png を作る
    if output_path is None:
        output_file = input_file.with_suffix(".png")
    else:
        output_file = Path(output_path)

    # Pillowで開いて保存
    with Image.open(input_file) as img:
        img.save(output_file, "PNG")
        print(f"変換完了: {output_file}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("使い方: python bmp_to_png.py input.bmp [output.png]")
    else:
        input_file = sys.argv[1]
        output_file = sys.argv[2] if len(sys.argv) > 2 else None
        convert_bmp_to_png(input_file, output_file)
