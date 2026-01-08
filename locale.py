import re
import os
import json
import collections

# --- 配置区 ---
# 扫描源码的路径
SOURCE_DIRS = ['./core', './source_modules', './sink_modules', './decoder_modules', './misc_modules']
LOCALES_DIR = './root/res/locales'             # JSON 存放路径
# 定义需要支持的语言：文件名 -> 初始提示语
LANG_CONFIG = {
    'zh_CN': '待翻译',
    'ja_JP': '未翻訳'
}
EXTRACT_PATTERN = r'_L\("([^"]+)"\)'  # 匹配代码中的 _L("...")

def extract_keys_from_code():
    keys = set()
    pattern = re.compile(EXTRACT_PATTERN)
    for s_dir in SOURCE_DIRS:
        if not os.path.exists(s_dir): continue
        for root, _, files in os.walk(s_dir):
            for file in files:
                if file.endswith(('.cpp', '.h', '.hpp')):
                    try:
                        with open(os.path.join(root, file), 'r', encoding='utf-8') as f:
                            keys.update(pattern.findall(f.read()))
                    except Exception as e:
                        print(f"读取文件 {file} 出错: {e}")
    return keys

def merge_locales():
    code_keys = extract_keys_from_code()
    print(f"从源码中提取到 {len(code_keys)} 个唯一 Key")

    if not os.path.exists(LOCALES_DIR):
        os.makedirs(LOCALES_DIR)

    for lang_code, placeholder in LANG_CONFIG.items():
        file_path = os.path.join(LOCALES_DIR, f"{lang_code}.json")
        existing_data = collections.OrderedDict()

        # 加载现有内容
        if os.path.exists(file_path):
            with open(file_path, 'r', encoding='utf-8') as f:
                try:
                    existing_data = json.load(f, object_pairs_hook=collections.OrderedDict)
                except:
                    pass

        # 合并逻辑
        new_dict = collections.OrderedDict()
        for key in sorted(code_keys): # 排序可以让生成的 JSON 在 Git 中对比更整齐
            if key in existing_data:
                new_dict[key] = existing_data[key] # 保留已翻译内容
            else:
                new_dict[key] = f"[{placeholder}] {key}" # 标记新 Key

        # 写入文件
        with open(file_path, 'w', encoding='utf-8') as f:
            json.dump(new_dict, f, ensure_ascii=False, indent=4)
        
        print(f"成功同步: {file_path} (当前共 {len(new_dict)} 条翻译)")

if __name__ == "__main__":
    merge_locales()