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

// AssetManager global
static AAssetManager* g_assetManager = nullptr;

extern "C"
JNIEXPORT void JNICALL
Java_com_s0mbr4xc_practica3vision_MainActivity_initAssetManager(
        JNIEnv* env, jobject, jobject assetManager) {
    g_assetManager = AAssetManager_fromJava(env, assetManager);
    if (!g_assetManager) LOGE("Error al obtener AssetManager");
    else LOGI("AssetManager inicializado");
}

// leer CSV desde assets
string leerCsvDesdeAssets(const char* filename) {
    if (!g_assetManager) return "";
    AAsset* asset = AAssetManager_open(g_assetManager, filename, AASSET_MODE_STREAMING);
    if (!asset) { LOGE("No se pudo abrir asset: %s", filename); return ""; }
    off_t len = AAsset_getLength(asset);
    string content(len, '\0');
    AAsset_read(asset, &content[0], len);
    AAsset_close(asset);
    return content;
}

// Bitmap -> gray Mat
Mat bitmapToMat(JNIEnv *env, jobject bitmap) {
    AndroidBitmapInfo info;
    void* pixels;
    if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) return Mat();
    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) return Mat();
    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0) return Mat();
    Mat rgba(info.height, info.width, CV_8UC4, pixels);
    Mat gray;
    cvtColor(rgba, gray, COLOR_RGBA2GRAY);
    AndroidBitmap_unlockPixels(env, bitmap);
    return gray;
}

// preprocesado: binariza e infla el contorno más grande
// preprocesado: binariza e infla el contorno más grande, ignorando el fondo
Mat preprocesarImagen(const Mat &gray) {
    Mat bin;
    // Binarización: trazos >127 → blanco, fondo negro
    threshold(gray, bin, 127, 255, THRESH_BINARY);

    // Cerrar pequeños huecos en el trazo
    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(5,5));
    morphologyEx(bin, bin, MORPH_CLOSE, kernel);

    // Extraer contornos
    vector<vector<Point>> contours;
    findContours(bin.clone(), contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    // Filtrar contornos que toquen los bordes (fondo)
    int w = bin.cols, h = bin.rows;
    vector<vector<Point>> validContours;
    for (auto &c : contours) {
        Rect r = boundingRect(c);
        if (r.x == 0 || r.y == 0 ||
            r.x + r.width  == w ||
            r.y + r.height == h)
            continue;
        validContours.push_back(c);
    }

    // Rellenar el contorno más grande de los válidos
    Mat filled = Mat::zeros(bin.size(), CV_8UC1);
    if (!validContours.empty()) {
        auto bestIt = max_element(validContours.begin(), validContours.end(),
                                  [](auto &a, auto &b){ return contourArea(a) < contourArea(b); });
        drawContours(filled, vector<vector<Point>>{*bestIt}, 0, Scalar(255), FILLED);
    }
    return filled;
}

// distancia euclídea en log-space
double calcularDistancia(const double hu1[7], const double hu2[7]) {
    double sum = 0;
    for (int i = 0; i < 7; ++i) {
        // a = log10(raw Hu)
        double a = hu1[i] == 0
                   ? 0
                   : log10(fabs(hu1[i]));
        // b = valor ya log10 leído del CSV
        double b = hu2[i];
        double diff = (a - b);
        sum += diff * diff;
    }
    return sqrt(sum);
}

// clasificación Hu desde CSV en assets
string clasificarConHu(const double hu[7]) {
    string data = leerCsvDesdeAssets("hu_dataset.csv");
    if (data.empty()) return "Error al leer CSV";

    // Mostrar en log las primeras 3 líneas para verificar
    istringstream preview(data);
    string line;
    for (int i = 0; i < 3 && getline(preview, line); ++i) {
        LOGI("CSV preview[%d]: %s", i, line.c_str());
    }

    istringstream ss(data);
    string bestClass = "Desconocida";
    double bestDist = 1e9, secondDist = 1e9;

    while (getline(ss, line)) {
        istringstream ls(line);
        string label;
        getline(ls, label, ',');
        double refHu[7];
        for (int i = 0; i < 7; ++i) {
            string v; getline(ls, v, ',');
            refHu[i] = stod(v);
        }
        double d = calcularDistancia(hu, refHu);

        // Tracking best y runner-up
        if (d < bestDist) {
            secondDist = bestDist;
            bestDist = d;
            bestClass = label;
        } else if (d < secondDist) {
            secondDist = d;
        }
    }

    LOGI("Mejor Hu → clase=%s, d=%.4f; runner-up d2=%.4f",
         bestClass.c_str(), bestDist, secondDist);

    char buf[64];
    snprintf(buf, sizeof(buf), "%.4f", bestDist);
    return bestClass + string(" (") + buf + ")";
}


string identificarPorVertices(const vector<Point>& contour) {
    vector<Point> approx;
    double peri = arcLength(contour, true);
    approxPolyDP(contour, approx, 0.04 * peri, true);

    int v = (int)approx.size();
    LOGI("approxPolyDP vertices=%d", v);
    if (v == 3) return "Triángulo";
    else if (v == 4) {
        Rect r = boundingRect(approx);
        float ar = float(r.width) / r.height;
        return (ar > 0.9 && ar < 1.1) ? "Cuadrado" : "Rectángulo";
    }
    else if (v > 7) return "Círculo";
    return "Desconocida";
}


extern "C"
JNIEXPORT jstring JNICALL
Java_com_s0mbr4xc_practica3vision_MainActivity_detectShapeFromBitmap(
        JNIEnv *env, jobject, jobject bitmap, jint descriptor) {
    Mat gray = bitmapToMat(env, bitmap);
    if (gray.empty()) return env->NewStringUTF("Error: imagen inválida");

    Mat proc = preprocesarImagen(gray);
    // Debug: volcar proc como PNG o vía ImageView en Java
    LOGI("preprocesado generado");

    vector<vector<Point>> contours;
    findContours(proc.clone(), contours, RETR_EXTERNAL, CHAIN_APPROX_NONE);
    LOGI("Número de contornos encontrados: %d", (int)contours.size());
    for (int i = 0; i < (int)contours.size(); ++i) {
        double area = contourArea(contours[i]);
        Rect r = boundingRect(contours[i]);
        LOGI(" C[%d]: area=%.1f bbox=(%d,%d,%d,%d)",
             i, area, r.x, r.y, r.width, r.height);
    }
    if (contours.empty()) return env->NewStringUTF("No se encontró contorno");

    // Elegir el contorno mayor
    int best = 0; double maxA = 0;
    for (int i = 0; i < (int)contours.size(); ++i) {
        double a = contourArea(contours[i]);
        if (a > maxA) { maxA = a; best = i; }
    }

    string result;
    if (descriptor == 0) {
        // Hu + comparación
        Mat norm;
        resize(proc, norm, Size(200,200));
        Moments m = moments(norm, true);
        double hu[7];
        HuMoments(m, hu);
        LOGI("Hu raw: [%e, %e, %e, %e, %e, %e, %e]",
             hu[0],hu[1],hu[2],hu[3],hu[4],hu[5],hu[6]);

        result = clasificarConHu(hu);

        // fallback: si la “confianza” es baja, usa vértices
        // (por ejemplo, runner-up − best < threshold)
        // result = identificarPorVertices(contours[best]);
    }
    else if (descriptor == 1) {
        // tu FFT… (igual que antes)
        Point2f center(0,0);
        for (auto &p : contours[best]) center += Point2f(p.x, p.y);
        center *= 1.0f / contours[best].size();
        vector<float> sig;
        for (auto &p : contours[best])
            sig.push_back(hypot(p.x - center.x, p.y - center.y));
        Mat sigMat = Mat(sig).reshape(1,1);
        sigMat.convertTo(sigMat, CV_32F);
        Mat fftRes;
        dft(sigMat, fftRes, DFT_COMPLEX_OUTPUT);
        float re = fftRes.at<Vec2f>(0)[0], im = fftRes.at<Vec2f>(0)[1];
        char buf[64];
        snprintf(buf, sizeof(buf), "FFT Re=%.2f, Im=%.2f", re, im);
        result = string(buf);
    }
    else {
        result = "Descriptor inválido";
    }

    return env->NewStringUTF(result.c_str());
}