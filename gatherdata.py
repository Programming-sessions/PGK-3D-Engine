import os

def _try_read_file(file_path):
    """Próbuje odczytać plik z użyciem popularnych kodowań w odpowiedniej kolejności."""
    # Kolejność:
    # 1. UTF-8 (bardzo popularne, obsługuje ASCII, brak BOM jest OK, jeśli jest próbowane jako pierwsze)
    # 2. UTF-16 (obsługuje BOM dla LE/BE; jeśli brak BOM, zachowanie może zależeć od platformy, ale Python często sobie radzi)
    #    Powinno poprawnie odczytać plik main.cpp z BOM FF FE (ÿþ).
    # 3. CP1250 (polskie/środkowoeuropejskie)
    # 4. Latin-1 (fallback, rzadko powoduje błąd, ale może źle zinterpretować)
    # 5. CP1252 (zachodnioeuropejskie Windows)
    encodings_to_try = [
        ('utf-8', "UTF-8"),
        ('utf-16', "UTF-16 (BOM-sensitive)"),
        ('cp1250', "CP1250 (Środkowoeuropejskie)"),
        ('latin-1', "Latin-1 (ISO-8859-1)"),
        ('cp1252', "CP1252 (Zachodnioeuropejskie Windows)")
    ]

    for encoding_val, encoding_name in encodings_to_try:
        try:
            with open(file_path, 'r', encoding=encoding_val) as f:
                # print(f"DEBUG: Pomyślnie odczytano {file_path} jako {encoding_name}") # Do debugowania
                return f.read()
        except UnicodeDecodeError:
            # print(f"DEBUG: Nie udało się zdekodować {file_path} jako {encoding_name}") # Do debugowania
            continue
        except Exception as e: # Inne błędy, np. brak uprawnień
            print(f"Błąd odczytu pliku {file_path} (kodowanie: {encoding_name}): {e}")
            return None # Zwróć None w przypadku błędu innego niż dekodowanie

    # Jeśli żadne kodowanie nie zadziałało
    tried_encodings_str = ", ".join([name for _, name in encodings_to_try])
    print(f"Nie udało się zdekodować pliku: {file_path} przy użyciu wypróbowanych kodowań ({tried_encodings_str}).")
    return None

def gather_specified_files(start_dir):
    """
    Zbiera zawartość z określonych plików i katalogów:
    - Pliki .cpp z katalogu start_dir (katalogu, w którym znajduje się skrypt).
    - Wszystkie pliki z katalogu start_dir/src i jego podkatalogów.
    - Wszystkie pliki z katalogu start_dir/assets/shaders i jego podkatalogów.
    Obsługuje różne kodowania plików.

    Args:
        start_dir (str): Katalog, od którego rozpoczyna się wyszukiwanie.

    Returns:
        dict: Słownik, w którym kluczami są ścieżki do plików (względne),
              a wartościami jest zawartość tych plików.
    """
    gathered_content = {}
    start_dir_abs = os.path.abspath(start_dir)
    src_dir_abs = os.path.join(start_dir_abs, "src")
    assets_shaders_dir_abs = os.path.join(start_dir_abs, "assets", "shaders")

    for root, _, files in os.walk(start_dir_abs, topdown=True):
        current_root_abs = os.path.abspath(root)

        for file_name in files:
            file_path_abs = os.path.join(current_root_abs, file_name)
            file_path_rel = os.path.relpath(file_path_abs, start_dir_abs)

            should_include = False

            if current_root_abs == start_dir_abs and file_name.endswith('.cpp'):
                should_include = True
            elif current_root_abs.startswith(src_dir_abs):
                should_include = True
            elif current_root_abs.startswith(assets_shaders_dir_abs):
                should_include = True

            if should_include:
                content = _try_read_file(file_path_abs)
                if content is not None:
                    gathered_content[file_path_rel] = content
    return gathered_content

def create_markdown_file(file_data, output_path):
    """
    Tworzy plik markdown z zebranych danych plików, używając kodowania utf-8 do zapisu.

    Args:
        file_data (dict): Słownik, gdzie klucze to ścieżki do plików, a wartości to ich zawartość.
        output_path (str): Ścieżka do wyjściowego pliku markdown.
    """
    with open(output_path, 'w', encoding='utf-8') as md_file:
        for file_path, content in file_data.items():
            md_file.write(f"## Plik: {file_path}\n")
            lang = ""
            if file_path.endswith((".cpp", ".cxx", ".cc")): lang = "cpp"
            elif file_path.endswith((".h", ".hpp", ".hxx")): lang = "cpp"
            elif file_path.endswith((".glsl", ".frag", ".vert", ".shader", ".geom", ".tesc", ".tese")): lang = "glsl"
            elif file_path.endswith(".py"): lang = "python"
            elif file_path.endswith(".js"): lang = "javascript"
            elif file_path.endswith(".html"): lang = "html"
            elif file_path.endswith(".css"): lang = "css"

            md_file.write(f"```{lang}\n")
            md_file.write(content)
            md_file.write("\n```\n\n")
    print(f"Zawartość plików została zapisana do: {output_path}")

if __name__ == "__main__":
    current_script_dir = "."
    all_gathered_files = gather_specified_files(current_script_dir)

    if all_gathered_files:
        create_markdown_file(all_gathered_files, "zebrane_pliki_poprawione.md")
    else:
        print("Nie znaleziono określonych plików w głównym katalogu, katalogu 'src' ani 'assets/shaders'.")