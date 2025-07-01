import cv2
import os
import numpy as np
import csv

DATASET_DIR = r'C:\Users\s3_xc\AndroidStudioProjects\Practica3Vision\all-images'
OUTPUT_CSV = 'hu_dataset.csv'

def preprocesar_imagen(img_path):
    img = cv2.imread(img_path, cv2.IMREAD_GRAYSCALE)
    if img is None:
        print(f"Error cargando {img_path}")
        return None

    _, binarizada = cv2.threshold(img, 127, 255, cv2.THRESH_BINARY_INV)

    # Rellenar contorno externo
    contornos, _ = cv2.findContours(binarizada.copy(), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    filled = np.zeros_like(binarizada)
    if contornos:
        cv2.drawContours(filled, contornos, 0, 255, thickness=cv2.FILLED)
        return filled
    return None

def extraer_momentos_hu(img):
    m = cv2.moments(img)
    hu = cv2.HuMoments(m).flatten()
    return hu

with open(OUTPUT_CSV, mode='w', newline='') as file:
    writer = csv.writer(file)

    for clase in os.listdir(DATASET_DIR):
        clase_dir = os.path.join(DATASET_DIR, clase)
        if not os.path.isdir(clase_dir):
            continue

        for nombre_img in os.listdir(clase_dir):
            ruta_img = os.path.join(clase_dir, nombre_img)
            pre = preprocesar_imagen(ruta_img)
            if pre is not None:
                hu = extraer_momentos_hu(pre)
                log_hu = [np.log10(abs(h) + 1e-10) for h in hu]  # log para normalizar
                writer.writerow([clase] + log_hu)
                print(f"Procesada: {ruta_img}")
