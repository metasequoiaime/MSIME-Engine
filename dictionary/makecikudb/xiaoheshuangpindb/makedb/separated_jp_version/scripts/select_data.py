import os.path
import sqlite3
import sys

local_app_data_dir = os.environ.get("LOCALAPPDATA")
db_path = os.path.join(
    local_app_data_dir, "metasequoiaime", "cutted_flyciku_with_jp.db"
)


def choose_tbl(sp_str: str) -> str:
    word_len = len(sp_str) // 2
    base_tbl = "tbl_{}_{}"
    return base_tbl.format(word_len if word_len < 8 else "others", sp_str[0])


def query_by_key(key: str):
    tbl_name = choose_tbl(key)
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    try:
        cursor.execute(
            f"SELECT name FROM sqlite_master WHERE type='table' AND name=?", (tbl_name,)
        )
        if cursor.fetchone() is None:
            print(f"Table {tbl_name} Not Exists")
            return
    except sqlite3.OperationalError as e:
        print(f"Exception when executing sql: {e}")
        return

    try:
        cursor.execute(
            f"SELECT key, jp, value, weight FROM {tbl_name} WHERE key = ? OR key LIKE ?",
            (key, f"{key}%"),
        )
        rows = cursor.fetchall()
        if not rows:
            print("No results found.")
        else:
            for row in rows:
                print(f"拼音: {row[0]}  简拼: {row[1]}  词: {row[2]}  权重: {row[3]}")
    except sqlite3.OperationalError as e:
        print(f"Exception when executing sql: {e}")
    finally:
        conn.close()


if __name__ == "__main__":
    query_key = "dyys"
    query_key = "yc"
    query_by_key(query_key)
