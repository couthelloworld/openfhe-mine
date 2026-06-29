#ifndef FHE_TRACER_H
#define FHE_TRACER_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <mutex>

// 定義追蹤事件的結構體
struct TraceEvent {
    std::string scheme;       // "CKKS", "TFHE", 或 "CORE" (共用數學層)
    std::string opLevel;      // "SCHEME" (高階) 或 "MATH" (低階)
    std::string opName;       // 操作名稱，例如 "EvalMult", "NTT", "PBS"
    uint32_t ringDimension;   // 多項式維度 N (例如 CKKS=65536, TFHE=1024)
    uint32_t numModuli;       // RNS 模數的數量 (Level)
    long long duration_ns;    // 執行時間 (奈秒)
    std::string timestamp;    // 紀錄順序或時間戳記
    
    // 將單一事件轉換為 JSON 格式字串
    std::string toJson() const {
        return "{\"scheme\":\"" + scheme + "\", \"opLevel\":\"" + opLevel + 
               "\", \"opName\":\"" + opName + "\", \"N\":" + std::to_string(ringDimension) + 
               ", \"L\":" + std::to_string(numModuli) + 
               ", \"duration_ns\":" + std::to_string(duration_ns) + "}";
    }
};

// 單例模式的 Tracer 類別
class FHETracer {
private:
    std::vector<TraceEvent> eventLog;
    std::mutex logMutex;
    FHETracer() {} // 隱藏建構子

public:
    // 取得全局唯一的 Tracer 實例
    static FHETracer& getInstance() {
        static FHETracer instance;
        return instance;
    }

    // 寫入一筆追蹤紀錄
    void logEvent(const std::string& scheme, const std::string& level, 
                  const std::string& name, uint32_t n, uint32_t l, long long duration) {
        std::lock_guard<std::mutex> lock(logMutex);
        eventLog.push_back({scheme, level, name, n, l, duration, ""});
    }

    // 將所有軌跡輸出到 JSON 檔案
    void dumpToJson(const std::string& filename) {
        std::lock_guard<std::mutex> lock(logMutex);
        std::ofstream outFile(filename);
        if (outFile.is_open()) {
            outFile << "[\n";
            for (size_t i = 0; i < eventLog.size(); ++i) {
                outFile << "  " << eventLog[i].toJson();
                if (i < eventLog.size() - 1) outFile << ",";
                outFile << "\n";
            }
            outFile << "]\n";
            outFile.close();
            std::cout << "[FHETracer] 軌跡已成功匯出至 " << filename << std::endl;
        }
    }
};

// 為了方便測量時間，寫一個 RAII 風格的計時器 Wrapper
class TraceTimer {
private:
    std::string scheme, level, name;
    uint32_t N, L;
    std::chrono::time_point<std::chrono::high_resolution_clock> start;
public:
    TraceTimer(std::string s, std::string l, std::string nm, uint32_t n, uint32_t modCount) 
        : scheme(s), level(l), name(nm), N(n), L(modCount) {
        start = std::chrono::high_resolution_clock::now();
    }
    ~TraceTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        FHETracer::getInstance().logEvent(scheme, level, name, N, L, duration);
    }
};

#endif // FHE_TRACER_H