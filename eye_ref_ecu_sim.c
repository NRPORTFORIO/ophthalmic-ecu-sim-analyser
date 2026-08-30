#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

#define SAMPLING_RATE_MS 10   // 10msループ（100Hz）
#define MEASURE_COUNT 50      // 50回（0.5秒分）の有効データを取得
#define LASER_MAX_POWER 100   // レーザー出力のハードウェア限界値（安全基準）
#define BLINK_THRESHOLD 10.0  // 瞬き（反射光ゼロ・データ欠落）と判定する閾値

// センサーデータ構造体
typedef struct {
    double raw_refraction;    // 測定生データ（屈折度）
    double filtered_refraction; // フィルタ後のデータ
    bool is_blinking;         // 瞬きフラグ
} EyeSensorData;

// 安全管理・制御状態構造体
typedef struct {
    int laser_power;          // 現在のレーザー出力（%）
    bool emergency_stop;      // 緊急停止フラグ
    int watchdog_counter;     // ウォッチドッグタイマ
} EcuSystemStatus;

// 擬似的な眼球データの生成（ノイズや瞬きをランダムに混入）
void read_eye_sensor(EyeSensorData *sensor, int step) {
    // 正常な眼球屈折度（例：-3.25Dの近視）をベースにする
    double base_diopter = -3.25;
    
    // 1. 20〜25ステップ目で「瞬き（データ遮断）」を擬似発生させる
    if (step >= 20 && step <= 25) {
        sensor->raw_refraction = 0.0; // 反射光が返ってこない状態
        return;
    }

    // 2. 通常の白色ノイズ（手の震えや涙液層の揺らぎ）を付与
    double noise = ((double)rand() / RAND_MAX - 0.5) * 0.4; // ±0.2Dのノイズ
    sensor->raw_refraction = base_diopter + noise;
}

// デジタルフィルタ（移動平均フィルタによるノイズ除去）
double apply_moving_average(double new_value, double *buffer, int size, int *index) {
    buffer[*index] = new_value;
    *index = (*index + 1) % size;

    double sum = 0;
    for (int i = 0; i < size; i++) {
        sum += buffer[i];
    }
    return sum / size;
}

int main() {
    srand((unsigned int)time(NULL));
    FILE *log_file = fopen("eye_measurement_log.csv", "w");
    if (log_file == NULL) {
        printf("エラー: ログファイルを開けませんでした。\n");
        return 1;
    }

    // CSVヘッダー書き込み
    fprintf(log_file, "Timestamp(ms),RawDiopter,FilteredDiopter,BlinkFlag,LaserPower,Status\n");

    // 制御バッファの初期化
    #define FILTER_SIZE 5
    double filter_buffer[FILTER_SIZE] = {0};
    int filter_index = 0;

    EcuSystemStatus ecu = { .laser_power = 50, .emergency_stop = false, .watchdog_counter = 0 };
    EyeSensorData sensor = {0};

    printf("--- ニデック向け: 眼球測定ECUシミュレーション開始 ---\n");

    int current_timestamp = 0;
    int valid_data_collected = 0;

    for (int step = 0; step < 80; step++) {
        current_timestamp = step * SAMPLING_RATE_MS;
        ecu.watchdog_counter = 0; // ウォッチドッグタイマをクリア（正常動作の証明）

        // 1. センサーデータ読み込み
        read_eye_sensor(&sensor, step);

        // 2. バリデーション ＆ 瞬き（ノイズ）判定
        // 測定値がゼロ（または極端に低い反射光）の場合は「瞬き」と判定
        if (sensor.raw_refraction > -BLINK_THRESHOLD && sensor.raw_refraction < BLINK_THRESHOLD && sensor.raw_refraction == 0.0) {
            sensor.is_blinking = true;
            sensor.filtered_refraction = 0.0; // 瞬き中は測定データを更新しない
            ecu.laser_power = 10; // 患者の安全のため、瞬き検知時はレーザーを減光（セーフモード）
        } else {
            sensor.is_blinking = false;
            ecu.laser_power = 50; // 通常出力
            // 移動平均フィルタ適用
            sensor.filtered_refraction = apply_moving_average(sensor.raw_refraction, filter_buffer, FILTER_SIZE, &filter_index);
            valid_data_collected++;
        }

        // 3. 超重要：フェイルセーフ（安全装置）のチェック
        // レーザー出力が何らかの異常で最大出力を超えた場合、即時シャットダウン
        if (ecu.laser_power > LASER_MAX_POWER) {
            ecu.emergency_stop = true;
            ecu.laser_power = 0;
        }

        // 4. データのCSVログ出力
        fprintf(log_file, "%d,%.2f,%.2f,%d,%d,%s\n",
                current_timestamp,
                sensor.raw_refraction,
                sensor.filtered_refraction,
                sensor.is_blinking,
                ecu.laser_power,
                ecu.emergency_stop ? "EMERGENCY" : (sensor.is_blinking ? "BLINK_HOLD" : "MEASURING")
        );

        // コンソール表示（リアルタイム風表示）
        printf("[%03dms] 生データ: %5.2f | フィルタ後: %5.2f | 状態: %s\n",
               current_timestamp, sensor.raw_refraction, sensor.filtered_refraction,
               sensor.is_blinking ? "瞬き検知（ホールド）" : "測定中");

        usleep(SAMPLING_RATE_MS * 1000); // 10ms待機
    }

    fclose(log_file);
    printf("--- シミュレーション終了。'eye_measurement_log.csv' にログを保存しました。 ---\n");
    return 0;
}
