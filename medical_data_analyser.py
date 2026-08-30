import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D

def analyze_medical_log():
    print("--- 医療用データ解析タスクを実行中 ---")
    
    # 1. ログファイルの読み込み
    try:
        df = pd.read_csv("eye_measurement_log.csv")
    except FileNotFoundError:
        print("エラー: eye_measurement_log.csv が見つかりません。C言語のECUを実行してください。")
        return

    # 2. データフィルタリング（瞬きフラグが立っている行を除外して純粋な測定値を抽出）
    valid_measures = df[df['BlinkFlag'] == 0]
    
    # 3. 診断サポート用データの算出
    mean_diopter = valid_measures['FilteredDiopter'].mean()
    max_diopter = valid_measures['FilteredDiopter'].max()
    min_diopter = valid_measures['FilteredDiopter'].min()
    
    # 簡易診断ロジック（ニデックの機器が自動で度数を算出する仕組みを模倣）
    diagnosis = "正視（正常）"
    if mean_diopter <= -3.0:
        diagnosis = "中等度近視"
    elif mean_diopter <= -0.5:
        diagnosis = "軽度近視"
    elif mean_diopter >= 0.5:
        diagnosis = "遠視"

    print(f"■ 分析結果レポート")
    print(f"  有効測定データ数: {len(valid_measures)} 件")
    print(f"  確定平均屈折度 (Diopter): {mean_diopter:.2f} D")
    print(f"  判定ステータス: {diagnosis}")
    
    # 4. 角膜トポグラフィ（形状可視化）の3Dグラフ擬似生成
    # 平均度数をベースに、乱視成分（角膜の歪み）をモデリングして3D描写
    x = np.linspace(-3, 3, 100)
    y = np.linspace(-3, 3, 100)
    X, Y = np.meshgrid(x, y)
    
    # 角膜の歪みを表す数式（中心が最も高く、周囲に向かって滑らかに下がるが、片側に少し歪んでいる乱視眼を再現）
    Z = mean_diopter - (X**2 + Y**2 * 1.3) * 0.1  
    
    fig = plt.figure(figsize=(12, 5))
    
    # 左側：測定時系列グラフ（生データ vs フィルタ後、および瞬き区間）
    ax1 = fig.add_subplot(1, 2, 1)
    ax1.plot(df['Timestamp(ms)'], df['RawDiopter'], label='Raw Data (with Noise)', color='lightgray', linestyle='--')
    ax1.plot(df['Timestamp(ms)'], df['FilteredDiopter'], label='Filtered Data (Moving Avg)', color='blue', linewidth=2)
    # 瞬き区間を赤くハイライト
    blink_zones = df[df['BlinkFlag'] == 1]
    if not blink_zones.empty:
        ax1.scatter(blink_zones['Timestamp(ms)'], blink_zones['RawDiopter'], color='red', label='Blink Detected', zorder=5)
        
    ax1.set_title("Real-time Eye Refraction Tracking")
    ax1.set_xlabel("Time (ms)")
    ax1.set_ylabel("Refraction (Diopter)")
    ax1.legend()
    ax1.grid(True)
    
    # 右側：角膜形状の3Dトポグラフィマップ
    ax2 = fig.add_subplot(1, 2, 2, projection='3d')
    surf = ax2.plot_surface(X, Y, Z, cmap='jet', edgecolor='none', alpha=0.8)
    ax2.set_title("Simulated Corneal Topography (3D Map)")
    ax2.set_xlabel("X Axis")
    ax2.set_ylabel("Y Axis")
    ax2.set_zlabel("Diopter Power")
    fig.colorbar(surf, ax=ax2, shrink=0.5, aspect=5)
    
    plt.tight_layout()
    plt.savefig("cornea_analysis_result.png")
    print("  -> 可視化結果を 'cornea_analysis_result.png' に保存しました。")
    plt.show()

if __name__ == "__main__":
    analyze_medical_log()
