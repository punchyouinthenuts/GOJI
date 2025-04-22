import os
from PyPDF2 import PdfReader, PdfWriter
from PyPDF2.generic import NameObject, TextStringObject

def get_pdf_files(directory):
    pdf_files = [f for f in os.listdir(directory) if f.lower().endswith('.pdf')]
    return pdf_files

def display_pdf_list(pdf_files):
    print("\nWHICH PDFs DO YOU WANT TO PROCESS?")
    for i, pdf in enumerate(pdf_files, 1):
        print(f"{i}. {os.path.splitext(pdf)[0]}")

def get_user_selection(pdf_files):
    while True:
        selection = input("\nEnter numbers separated by commas (e.g., 1,2,3): ").strip()
        try:
            numbers = [int(n.strip()) for n in selection.split(',')]
            if all(1 <= n <= len(pdf_files) for n in numbers):
                return numbers
            else:
                print("YOU MAY ONLY SELECT FROM THE NUMBERED LIST")
        except ValueError:
            print("YOU MAY ONLY SELECT FROM THE NUMBERED LIST")

def confirm_selection(selected_files):
    print("\nYOU WANT TO PROCESS")
    for i, file in enumerate(selected_files, 1):
        print(f"{i}. {os.path.splitext(file)[0]}")
    
    while True:
        confirm = input("\nIS THIS CORRECT? Y/N: ").upper()
        if confirm in ['Y', 'N']:
            return confirm == 'Y'

def modify_pdf_links(pdf_path, output_path):
    reader = PdfReader(pdf_path)
    writer = PdfWriter()
    
    for page_num in range(len(reader.pages)):
        page = reader.pages[page_num]
        if '/Annots' in page:
            annotations = page['/Annots']
            
            for annotation_ref in annotations:
                annotation = annotation_ref.get_object()
                if '/A' in annotation and '/S' in annotation['/A']:
                    if annotation['/A']['/S'] == '/URI':
                        link = annotation['/A']['/URI']
                        if not (link.startswith('http://') or link.startswith('https://')):
                            annotation['/A'][NameObject('/S')] = NameObject('/Launch')
                            annotation['/A'][NameObject('/F')] = TextStringObject(link)
        writer.add_page(page)
    
    with open(output_path, 'wb') as output_file:
        writer.write(output_file)

def main():
    directory = r"C:\Users\JCox\Desktop\AUTOMATION\WORD PDFs"
    
    pdf_files = get_pdf_files(directory)
    if not pdf_files:
        print("NO PDF FILES IN TARGET FOLDER! Press any key to terminate...")
        input()
        return
    
    while True:
        display_pdf_list(pdf_files)
        selected_numbers = get_user_selection(pdf_files)
        selected_files = [pdf_files[i-1] for i in selected_numbers]
        
        if confirm_selection(selected_files):
            for pdf_file in selected_files:
                input_path = os.path.join(directory, pdf_file)
                output_path = os.path.join(directory, f"{os.path.splitext(pdf_file)[0]}_modified.pdf")
                modify_pdf_links(input_path, output_path)
            break

if __name__ == "__main__":
    main()
