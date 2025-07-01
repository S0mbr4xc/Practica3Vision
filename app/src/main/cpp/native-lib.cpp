#include <jni.h>
#include <string>
#include <android/bitmap.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <opencv2/opencv.hpp>
#include <vector>
#include <cmath>
#include <sstream>

using namespace cv;
using namespace std;

#define LOG_TAG "NativeLib"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ========================
// GLOBAL: AssetManager
static AAssetManager* g_assetManager = nullptr;

// ========================
// Inicializar AssetManager
extern "C"
JNIEXPORT void JNICALL
Java_com_s0mbr4xc_practica3vision_MainActivity_initAssetManager(
        JNIEnv* env, jobject, jobject assetManager) {
    g_assetManager = AAssetManager_fromJava(env, assetManager);
    if (!g_assetManager) {
        LOGE("Error al obtener AssetManager");
    } else {
        LOGI("AssetManager inicializado correctamente");
    }
}

// ========================
// Leer CSV desde assets
string leerCsvDesdeAssets(const char* filename) {
    if (!g_assetManager) {
        LOGE("AssetManager no inicializado");
        return "";
    }
    AAsset* asset = AAssetManager_open(g_assetManager, filename, AASSET_MODE_STREAMING);
    if (!asset) {
        LOGE("No se pudo abrir asset: %s", filename);
        return "";
    }

    off_t length = AAsset_getLength(asset);
    string content(length, '\0');
    AAsset_read(asset, content.data(), length);
    AAsset_close(asset);
    return content;
}

// ========================
// Conversión de Bitmap a Mat (escala de grises)
Mat bitmapToMat(JNIEnv *env, jobject bitmap) {
    AndroidBitmapInfo info;
    void *pixels;
    if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) return Mat();
    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) return Mat();
    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0) return Mat();

    Mat rgba(info.height, info.width, CV_8UC4, pixels);
    Mat gray;
    cvtColor(rgba, gray, COLOR_RGBA2GRAY);
    AndroidBitmap_unlockPixels(env, bitmap);
    return gray;
}

// ========================
// Preprocesar imagen
Mat preprocesarImagen(const Mat &gray) {
    Mat bin;
    threshold(gray, bin, 127, 255, THRESH_BINARY_INV);
    vector<vector<Point>> contours;
    findContours(bin.clone(), contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    Mat filled = Mat::zeros(bin.size(), CV_8UC1);
    if (!contours.empty()) {
        drawContours(filled, contours, 0, Scalar(255), FILLED);
    }
    return filled;
}

// ========================
// Distancia euclídea log-espacio
double calcularDistancia(const double hu1[7], const double hu2[7]) {
    double sum = 0.0;
    for (int i = 0; i < 7; i++) {
        double a = hu1[i] == 0 ? 0 : log10(fabs(hu1[i]));
        double b = hu2[i] == 0 ? 0 : log10(fabs(hu2[i]));
        sum += (a - b) * (a - b);
    }
    return sqrt(sum);
}

// ========================
// Clasificar con Hu desde CSV en assets
string clasificarConHu(const double hu[7]) {
    string contenido = leerCsvDesdeAssets("hu_dataset.csv");
    if (contenido.empty()) return "Error al leer CSV";

    istringstream ss(contenido);
    string linea;
    string mejorClase = "Desconocida";
    double mejorDist = 1e9;

    while (getline(ss, linea)) {
        istringstream ls(linea);
        string etiqueta;
        getline(ls, etiqueta, ',');
        double huRef[7];
        for (int i = 0; i < 7; i++) {
            string val;
            getline(ls, val, ',');
            huRef[i] = stod(val);
        }
        double dist = calcularDistancia(hu, huRef);
        if (dist < mejorDist) {
            mejorDist = dist;
            mejorClase = etiqueta;
        }
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.4f", mejorDist);
    return mejorClase + " (" + string(buf) + ")";
}

// ========================
// Método principal
extern "C"
JNIEXPORT jstring JNICALL
Java_com_s0mbr4xc_practica3vision_MainActivity_detectShapeFromBitmap(
        JNIEnv *env, jobject, jobject bitmap, jint descriptor) {

    Mat gray = bitmapToMat(env, bitmap);
    if (gray.empty()) return env->NewStringUTF("Error al convertir bitmap");

    Mat proc = preprocesarImagen(gray);
    vector<vector<Point>> contours;
    findContours(proc.clone(), contours, RETR_EXTERNAL, CHAIN_APPROX_NONE);
    if (contours.empty()) return env->NewStringUTF("No se encontró contorno");

    string result;

    if (descriptor == 0) {
        Moments m = moments(contours[0]);
        if (m.m00 == 0.0) return env->NewStringUTF("Error: figura vacía");
        double hu[7];
        HuMoments(m, hu);
        result = clasificarConHu(hu);

    } else if (descriptor == 1) {
        vector<Point> contour = contours[0];
        Point2f center(0, 0);
        for (auto &p : contour) center += Point2f((float)p.x, (float)p.y);
        center *= 1.0f / contour.size();

        vector<float> signature;
        for (auto &p : contour) signature.push_back(hypot(p.x - center.x, p.y - center.y));

        Mat sigMat = Mat(signature).reshape(1,1);
        sigMat.convertTo(sigMat, CV_32F);
        Mat fftRes;
        dft(sigMat, fftRes, DFT_COMPLEX_OUTPUT);
        float re = fftRes.at<Vec2f>(0)[0], im = fftRes.at<Vec2f>(0)[1];

        char buf[64];
        snprintf(buf, sizeof(buf), "FFT Re=%.2f, Im=%.2f", re, im);
        result = string(buf);
    } else {
        result = "Descriptor inválido";
    }

    return env->NewStringUTF(result.c_str());
}
