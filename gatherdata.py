import os

def gather_code_files(start_dir):
    """
    Gathers content from .cpp and .h files in a directory and its subdirectories using latin-1 encoding.

    Args:
        start_dir (str): The directory to start the search from.

    Returns:
        dict: A dictionary where keys are file paths and values are the content of the files.
    """
    code_files = {}
    for root, _, files in os.walk(start_dir):
        # Only process files in the current directory or subdirectories of 'src'
        if root == start_dir or root.startswith(os.path.join(start_dir, "src")):
            for file in files:
                if file.endswith(('.cpp', '.h')):
                    file_path = os.path.join(root, file)
                    try:
                        with open(file_path, 'r', encoding='latin-1') as f:
                            code_files[file_path] = f.read()
                    except Exception as e:
                        print(f"Error reading file: {file_path} - {e}")
    return code_files

def create_markdown_file(code_data, output_path):
    """
    Creates a markdown file from the gathered code data using latin-1 encoding for writing.

    Args:
        code_data (dict): A dictionary where keys are file paths and values are the content of the files.
        output_path (str): The path to the output markdown file.
    """
    with open(output_path, 'w', encoding='latin-1') as md_file:
        for file_path, content in code_data.items():
            md_file.write(f"## {file_path}\n")
            md_file.write(content)
            md_file.write("\n\n")
    print(f"Code content written to: {output_path}")

if __name__ == "__main__":
    current_dir = os.getcwd()
    all_code = {}

    # Gather files from the main directory and the 'src' subdirectory
    code_in_main = gather_code_files(current_dir)
    all_code.update(code_in_main)

    if all_code:
        create_markdown_file(all_code, "code.md")
    else:
        print("No .cpp or .h files found in the main directory or the 'src' subdirectory.")