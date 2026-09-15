#include <DxLib.h>

static const void LogMessage(const char* message, const char* fileName = "MyLog.txt")
{
    // 画面に
    printfDx("%s\n", message);

    // デバッグウィンドウ
    OutputDebugStringA(message);
    OutputDebugStringA("\n");

    // ファイルにも
    FILE* fp = nullptr;
    fopen_s(&fp, fileName, "a");

    if (fp) {
        fprintf(fp, "%s\n", message);
        fclose(fp);
    }
}
