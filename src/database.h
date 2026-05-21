#pragma once
#include <windows.h>

// SQLite类型定义（简化版本，因为我们使用动态加载）
typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;
typedef int (*sqlite3_callback)(void*, int, char**, char**);
typedef long long sqlite3_int64;

#define SQLITE_OK               0
#define SQLITE_ERROR            1
#define SQLITE_ROW              100
#define SQLITE_DONE             101
#define SQLITE_TRANSIENT        ((void(*)(void*))-1)

// ── 错误代码 ────────────────────────────────────────────────
#define DB_OK               0
#define DB_ERROR_OPEN      -1
#define DB_ERROR_CREATE    -2
#define DB_ERROR_EXEC      -3
#define DB_ERROR_QUERY     -4
#define DB_ERROR_MEMORY    -5
#define DB_ERROR_NOT_FOUND -6

// ── 数据库操作上下文 ───────────────────────────────────────
typedef struct {
    HMODULE     hDll;
    sqlite3    *db;
    wchar_t     dbPath[MAX_PATH];
} DbContext;

// ── 函数指针类型定义 ───────────────────────────────────────
typedef int (*sqlite3_open_ptr)(const char *filename, sqlite3 **ppDb);
typedef int (*sqlite3_open16_ptr)(const void *filename, sqlite3 **ppDb);
typedef int (*sqlite3_close_ptr)(sqlite3 *db);
typedef int (*sqlite3_exec_ptr)(sqlite3*, const char *sql, sqlite3_callback, void *, char **errmsg);
typedef int (*sqlite3_prepare_v2_ptr)(sqlite3 *db, const char *zSql, int nByte, sqlite3_stmt **ppStmt, const char **pzTail);
typedef int (*sqlite3_step_ptr)(sqlite3_stmt*);
typedef int (*sqlite3_finalize_ptr)(sqlite3_stmt*);
typedef const unsigned char* (*sqlite3_column_text_ptr)(sqlite3_stmt*, int iCol);
typedef int (*sqlite3_bind_text_ptr)(sqlite3_stmt*, int, const char*, int n, void(*)(void*));
typedef sqlite3_int64 (*sqlite3_last_insert_rowid_ptr)(sqlite3*);

// ── 导出函数声明 ───────────────────────────────────────────

#ifdef __cplusplus
extern "C" {
#endif

// 初始化数据库
int Db_Init(DbContext *ctx, const wchar_t *dbPath);

// 关闭数据库
int Db_Close(DbContext *ctx);

// 插入题目和答案
int Db_Insert(DbContext *ctx, const wchar_t *question, const wchar_t *answer);

// 搜索答案（通过题目）
wchar_t* Db_Search(DbContext *ctx, const wchar_t *question);

// 模糊搜索答案（先精确匹配，再子串匹配）
wchar_t* Db_SearchFuzzy(DbContext *ctx, const wchar_t *question);

// 删除记录（通过题目）
int Db_Delete(DbContext *ctx, const wchar_t *question);

// 获取所有记录（用于列表显示）
struct QAPair {
    wchar_t *question;
    wchar_t *answer;
};
struct QAPair* Db_GetAllPairs(DbContext *ctx, int *count);

// 释放查询结果
void Db_FreeResults(struct QAPair *pairs, int count);

// 从文本内容导入（格式：题目:题目内容 答案:答案内容）
int Db_ImportFromText(DbContext *ctx, const wchar_t *text);

// 将所有记录导出到文本（格式：题目:题目内容 答案:答案内容）
wchar_t* Db_ExportToText(DbContext *ctx);

// 检查SQLite DLL是否已加载
BOOL Db_IsInitialized(DbContext *ctx);

#ifdef __cplusplus
}
#endif
