// ゲームスレッド（遮蔽の幾何計算）とオーディオレンダースレッド（音声処理）へ渡すためのクラス群
//
// 両者は別スレッドで動くため直接参照させず、ゲームスレッドが目標値を書き込み、オーディオスレッドがそれを読む
#pragma once

#include "CoreMinimal.h"

struct FOcclusionParams
{
    float CutoffHz   = 20000.0f; // 遮蔽なし 
    float LinearGain = 1.0f;     // 遮蔽なし&減衰なし
};

class FOcclusionRegistry
{
public:
    static FOcclusionRegistry& Get()
    {
        static FOcclusionRegistry Instance;
        return Instance;
    }

    // ゲームスレッドから呼ぶ
    void SetParams(uint64 SourceKey, const FOcclusionParams& Params)
    {
        FScopeLock Lock(&Mutex);
        Map.FindOrAdd(SourceKey) = Params;
    }

    // オーディオレンダースレッドから呼ぶ
    bool GetParams(uint64 SourceKey, FOcclusionParams& OutParams) const
    {
        FScopeLock Lock(&Mutex);
        if (const FOcclusionParams* Found = Map.Find(SourceKey))
        {
            OutParams = *Found;
            return true;
        }
        return false;
    }

private:
    // ロックが保護するのは float 2つのコピー
    mutable FCriticalSection Mutex;
    TMap<uint64, FOcclusionParams> Map;
};
