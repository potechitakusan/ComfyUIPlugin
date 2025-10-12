import argparse
import os
from PIL import Image

def convert_png_to_bmp(input_path):
    # PNG画像を読み込み、ビットマップに変換し、拡張子を.bmpに変更して保存する。
    
    # 1. 画像のオープン
    try:
        img = Image.open(input_path)
    except FileNotFoundError:
        print(f"エラー: ファイルが見つかりません - {input_path}")
        return
    except Exception as e:
        print(f"エラー: 画像ファイルの読み込み中に問題が発生しました - {e}")
        return

    # 2. 透過情報を持つPNGの処理 (重要)
    # BMPは通常アルファチャンネル（透過情報）をサポートしません。
    # 透過PNGをBMPに変換する場合、アルファチャンネルを破棄するために 'RGB' に変換します。
    if img.mode == 'RGBA':
        img = img.convert('RGB')

    # 3. 出力ファイルのパスを生成
    # 拡張子を除いたファイル名を取得
    base_name = os.path.splitext(os.path.basename(input_path))[0]
    # 入力ファイルと同じディレクトリに出力
    output_dir = os.path.dirname(input_path)
    if not output_dir:
        output_dir = '.'
        
    output_path = os.path.join(output_dir, f"{base_name}.bmp")

    # 4. BMP形式で保存
    try:
        img.save(output_path, 'BMP')
        print(f"変換が完了しました: {input_path} -> {output_path}")
    except Exception as e:
        print(f"エラー: BMPファイルの保存中に問題が発生しました - {e}")

if __name__ == '__main__':
    # コマンドライン引数の設定
    parser = argparse.ArgumentParser(
        description="PNG画像をBMP画像に変換するスクリプト。"
    )
    # 必須の引数として入力ファイルのパスを追加
    parser.add_argument(
        'input_file',
        type=str,
        help='変換したいPNG画像ファイルのパスを指定してください。'
    )
    
    args = parser.parse_args()
    
    # 変換関数の実行
    convert_png_to_bmp(args.input_file)
