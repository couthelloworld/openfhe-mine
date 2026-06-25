#include "binfhecontext.h"
#include <chrono>
#include <iostream>

using namespace lbcrypto;
using namespace std;
using namespace std::chrono;

int main() {
    cout << "================================================================" << endl;
    cout << "  OpenFHE LMKCDEY Benchmark (Targeting Hardware Profiling) " << endl;
    cout << "================================================================" << endl;

    // 1. 初始化環境 (設定為 LMKCDEY 演算法)
    auto cc = BinFHEContext();
    // 使用 STD128 安全級別，並明確指定使用 LMKCDEY Bootstrapping 演算法
    cc.GenerateBinFHEContext(STD128_LMKCDEY, LMKCDEY);
    
    cout << "[System] BinFHE Context Generated with LMKCDEY method." << endl;

    // =================================================================
    // 階段一：金鑰生成 (Key Generation) - 這是硬體初始化與記憶體配置的關鍵
    // =================================================================
    
    cout << "\n--- Phase 1: Key Generation ---" << endl;
    auto start_keygen = high_resolution_clock::now();
    
    // 產生 LWE 私鑰
    auto sk = cc.KeyGen();
    
    // 產生 Bootstrapping Key (BSK) 與 Key Switching Key (KSK)
    // 這裡會產生我們討論過那幾十 MB 的常數矩陣
    cc.BTKeyGen(sk);
    
    auto stop_keygen = high_resolution_clock::now();
    auto duration_keygen = duration_cast<milliseconds>(stop_keygen - start_keygen);
    cout << "BTKeyGen (BSK + KSK) Time: " << duration_keygen.count() << " ms" << endl;

    // =================================================================
    // 階段二：加密 (Encryption)
    // =================================================================
    cout << "\n--- Phase 2: Encryption ---" << endl;
    auto start_enc = high_resolution_clock::now();
    
    // 加密兩個輸入位元： bit1 = 1, bit2 = 0
    auto ct1 = cc.Encrypt(sk, 1);
    auto ct2 = cc.Encrypt(sk, 0);
    
    auto stop_enc = high_resolution_clock::now();
    auto duration_enc = duration_cast<microseconds>(stop_enc - start_enc);
    // 除以 2 得到單次加密時間
    cout << "Encryption Time (per bit): " << duration_enc.count() / 2.0 << " us" << endl;

    // =================================================================
    // 階段三：邏輯閘評估 (Gate Evaluation) - 硬體加速器的核心戰場
    // =================================================================
    cout << "\n--- Phase 3: Gate Evaluation (NAND Gate) ---" << endl;
    
    int num_iterations = 100; // 跑 100 次來抓平均值，消除 OS 背景排程的誤差
    cout << "Running " << num_iterations << " iterations for stable profiling..." << endl;
    
    LWECiphertext ct_result;
    
    auto start_eval = high_resolution_clock::now();
    
    for (int i = 0; i < num_iterations; ++i) {
        // 執行同態 NAND 閘運算
        // 這裡面包含了 LMKCDEY 最核心的 Automorphism 洗牌與 Key Switching
        ct_result = cc.EvalBinGate(NAND, ct1, ct2);
    }
    
    auto stop_eval = high_resolution_clock::now();
    auto duration_eval = duration_cast<milliseconds>(stop_eval - start_eval);
    double avg_eval_time = static_cast<double>(duration_eval.count()) / num_iterations;
    
    cout << "Average EvalBinGate Time : " << avg_eval_time << " ms / gate" << endl;

    // =================================================================
    // 階段四：解密與驗證 (Decryption & Verification)
    // =================================================================
    cout << "\n--- Phase 4: Decryption & Verification ---" << endl;
    LWEPlaintext result;
    cc.Decrypt(sk, ct_result, &result);
    
    // NAND(1, 0) 應該要是 1
    cout << "Input: 1 NAND 0" << endl;
    cout << "Decrypted Result: " << result << ((result == 1) ? " (PASS)" : " (FAIL)") << endl;
    cout << "================================================================" << endl;

    // =================================================================
    // 階段五：硬體架構分析 - Automorphism 記憶體交換位址追蹤 (Routing Table)
    // =================================================================
    cout << "\n--- Phase 5: Hardware Memory Routing Data (Automorphism) ---" << endl;
    
    // 取得當前的多項式長度 N
    uint32_t N = cc.GetParams()->GetLWEParams()->GetN();
    uint32_t M = 2 * N;
    
    // 假設我們要做一次 k=5 的 Automorphism (這是最常見的基礎旋轉)
    uint32_t k = 5; 
    
    cout << "[Hardware Spec] Ring Dimension (N): " << N << ", Modulus (M): " << M << endl;
    cout << "Showing memory index swap for Automorphism (k=" << k << ")...\n" << endl;
    
    cout << "Original Index (SRAM Read) -> New Index (SRAM Write)" << endl;
    cout << "----------------------------------------------------" << endl;
    
    // 為了不洗版，我們只印出前 10 筆和最後 2 筆的記憶體交換軌跡
    for (uint32_t i = 1; i < M; i += 2) { // 只有奇數索引有資料
        uint32_t new_index = (i * k) % M;
        
        // 由於真實多項式只有 N 個係數，我們需要把 0~M 映射回 0~N-1
        uint32_t read_addr = i >> 1;
        uint32_t write_addr = new_index >> 1;
        
        if (read_addr < 10 || read_addr >= N - 2) {
            cout << "  SRAM_Addr[" << read_addr << "]\t ---> \tSRAM_Addr[" << write_addr << "]" << endl;
        }
        if (read_addr == 10) {
            cout << "\t...\t ---> \t..." << endl;
        }
    }
    cout << "----------------------------------------------------" << endl;


    return 0;
}