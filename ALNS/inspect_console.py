import re

def main():
    path = "my_output.txt"
    try:
        with open(path, 'r', encoding='utf-16') as f:
            content = f.read()
    except:
        with open(path, 'r', encoding='utf-8') as f:
            content = f.read()
            
    lines = content.split('\n')
    for line in lines:
        if "Vehicle 29:" in line or "Vehicle 20:" in line or "Vehicle 21:" in line:
            print(f"FOUND: {line[:200]}") 
if __name__ == "__main__":
    main()

