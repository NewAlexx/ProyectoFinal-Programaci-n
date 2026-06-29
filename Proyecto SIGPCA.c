/* SIGPCA - Sistema Integral de Gestion y Prediccion de Contaminacion
   del Aire en Zonas Urbanas (Quito).  Programacion 1 - ISWZ1102.
   Un solo archivo. Usa funciones, punteros, arreglos, matrices y
   estructuras. Sin memoria dinamica, sin librerias externas. */
#include <stdio.h>
#include <string.h>

#define ZONAS   5      /* numero de zonas monitoreadas        */
#define DIAS    30     /* dias de historico por zona          */
#define VENTANA 7      /* dias usados en el promedio ponderado*/

/* Limites OMS para 24 h (PM2.5/NO2/SO2 en ug/m3, CO en mg/m3) */
#define L_PM25 15.0f
#define L_NO2  25.0f
#define L_SO2  40.0f
#define L_CO    4.0f

/* ---- Estructuras ---- */
typedef struct { float pm25, no2, so2, co; } Contaminantes;
typedef struct { float temp, viento, humedad; } Clima;
typedef struct {
    char          nombre[20];
    Contaminantes historico[DIAS];   /* matriz ZONAS x DIAS de structs */
    Clima         clima;
    Contaminantes actual;            /* lectura del dia mas reciente   */
    Contaminantes prediccion;        /* niveles estimados a 24 h       */
} Zona;

/* ---- Carga de datos (red REMMAQ del Municipio de Quito) ---- */
void inicializar(Zona z[]) {
    const char *nom[ZONAS] = {"Carapungo","Cotocollao","Belisario",
                              "El Camal","Los Chillos"};
    float bpm[ZONAS]  = {10.0f, 13.5f, 11.0f, 15.2f, 13.0f};
    float bno2[ZONAS] = {18.0f, 19.0f, 22.0f, 30.0f, 17.0f};
    float bso2[ZONAS] = { 6.5f,  8.0f,  9.5f, 12.0f,  9.0f};
    float bco[ZONAS]  = { 0.8f,  1.0f,  1.2f,  1.8f,  1.0f};
    /* clima por zona: temperatura, viento, humedad */
    Clima clm[ZONAS] = {
        {14.0f,2.5f,65.0f},{15.0f,2.2f,68.0f},{16.0f,2.0f,60.0f},
        {16.0f,1.5f,68.0f},{17.0f,1.0f,78.0f}
    };
    int i, d;
    for (i = 0; i < ZONAS; i++) {
        strcpy(z[i].nombre, nom[i]);
        z[i].clima = clm[i];
        for (d = 0; d < DIAS; d++) {              /* leve variacion diaria */
            float r = (d % 7) * 0.1f;
            z[i].historico[d].pm25 = bpm[i]  + r;
            z[i].historico[d].no2  = bno2[i] + r;
            z[i].historico[d].so2  = bso2[i];
            z[i].historico[d].co   = bco[i];
        }
        z[i].actual = z[i].historico[DIAS - 1];   /* dia mas reciente */
    }
}

/* ---- Calculos ---- */
/* copia un contaminante (0=PM2.5,1=NO2,2=SO2,3=CO) a un arreglo */
void serie(const Zona *z, int campo, float out[]) {
    int d;
    for (d = 0; d < DIAS; d++) {
        const Contaminantes *c = &z->historico[d];
        out[d] = (campo==0)?c->pm25:(campo==1)?c->no2:(campo==2)?c->so2:c->co;
    }
}
/* promedio aritmetico simple de un arreglo */
float promedio(const float v[], int n) {
    int i; float s = 0.0f;
    for (i = 0; i < n; i++) s += v[i];
    return s / n;
}
/* promedio movil ponderado: mas peso a los dias recientes (1..ventana) */
float pmp(const float v[], int n, int ventana) {
    int i, peso = 1, suma = 0; float num = 0.0f;
    if (ventana > n) ventana = n;
    for (i = n - ventana; i < n; i++) { num += peso * v[i]; suma += peso; peso++; }
    return num / suma;
}
/* factor climatico: viento dispersa; humedad y frio concentran */
float factorClima(const Clima *c, int esPM25) {
    float kv = 0.30f, kt = 0.25f, kh = esPM25 ? 0.40f : 0.20f;
    float fv = 1.0f + kv*(2.0f - c->viento)/2.0f;
    float fh = 1.0f + kh*(c->humedad - 50.0f)/100.0f;
    float ft = 1.0f + kt*(18.0f - c->temp)/18.0f;
    if (fv < 0.6f) fv = 0.6f;
    if (fv > 1.4f) fv = 1.4f;
    return fv * fh * ft;
}
/* prediccion a 24 h = PMP(historico) * factor climatico (puntero a zona) */
void predecir(Zona *z) {
    float v[DIAS];
    serie(z,0,v); z->prediccion.pm25 = pmp(v,DIAS,VENTANA)*factorClima(&z->clima,1);
    serie(z,1,v); z->prediccion.no2  = pmp(v,DIAS,VENTANA)*factorClima(&z->clima,0);
    serie(z,2,v); z->prediccion.so2  = pmp(v,DIAS,VENTANA)*factorClima(&z->clima,0);
    serie(z,3,v); z->prediccion.co   = pmp(v,DIAS,VENTANA)*factorClima(&z->clima,0);
}
/* devuelve "OK" o "EXCEDE" segun el limite */
const char* estado(float valor, float limite) {
    return valor > limite ? "EXCEDE" : "OK";
}

/* ---- Opciones del menu ---- */
void monitoreo(const Zona z[]) {
    int i;
    printf("\nMONITOREO DE NIVELES ACTUALES (vs limites OMS)\n");
    for (i = 0; i < ZONAS; i++) {
        const Contaminantes *a = &z[i].actual;
        printf("\nZona: %s\n", z[i].nombre);
        printf("  PM2.5 = %5.1f ug/m3  [%s]\n", a->pm25, estado(a->pm25,L_PM25));
        printf("  NO2   = %5.1f ug/m3  [%s]\n", a->no2,  estado(a->no2, L_NO2));
        printf("  SO2   = %5.1f ug/m3  [%s]\n", a->so2,  estado(a->so2, L_SO2));
        printf("  CO    = %5.2f mg/m3  [%s]\n", a->co,   estado(a->co,  L_CO));
    }
}
void promedios(const Zona z[]) {
    int i; float v[DIAS];
    printf("\nPROMEDIO HISTORICO 30 DIAS (vs limites OMS)\n");
    for (i = 0; i < ZONAS; i++) {
        printf("\nZona: %s\n", z[i].nombre);
        serie(&z[i],0,v); printf("  PM2.5 = %5.1f  [%s]\n", promedio(v,DIAS), estado(promedio(v,DIAS),L_PM25));
        serie(&z[i],1,v); printf("  NO2   = %5.1f  [%s]\n", promedio(v,DIAS), estado(promedio(v,DIAS),L_NO2));
        serie(&z[i],2,v); printf("  SO2   = %5.1f  [%s]\n", promedio(v,DIAS), estado(promedio(v,DIAS),L_SO2));
        serie(&z[i],3,v); printf("  CO    = %5.2f  [%s]\n", promedio(v,DIAS), estado(promedio(v,DIAS),L_CO));
    }
}
void prediccion(Zona z[]) {
    int i;
    printf("\nPREDICCION A 24 HORAS (vs limites OMS)\n");
    for (i = 0; i < ZONAS; i++) {
        predecir(&z[i]);
        const Contaminantes *p = &z[i].prediccion;
        printf("\nZona: %s  (T=%.0fC Viento=%.1fm/s Hum=%.0f%%)\n",
               z[i].nombre, z[i].clima.temp, z[i].clima.viento, z[i].clima.humedad);
        printf("  PM2.5 = %5.1f  [%s]\n", p->pm25, estado(p->pm25,L_PM25));
        printf("  NO2   = %5.1f  [%s]\n", p->no2,  estado(p->no2, L_NO2));
        printf("  SO2   = %5.1f  [%s]\n", p->so2,  estado(p->so2, L_SO2));
        printf("  CO    = %5.2f  [%s]\n", p->co,   estado(p->co,  L_CO));
    }
}
void alertas(Zona z[]) {
    int i, hay = 0;
    printf("\nALERTAS PREVENTIVAS (segun prediccion 24 h)\n");
    for (i = 0; i < ZONAS; i++) {
        predecir(&z[i]);
        const Contaminantes *p = &z[i].prediccion;
        if (p->pm25>L_PM25 || p->no2>L_NO2 || p->so2>L_SO2 || p->co>L_CO) {
            printf("  [!] ALERTA - %s: se preve exceder limites.\n", z[i].nombre);
            if (p->pm25 > L_PM25)
                printf("      Recomendacion: usar mascarilla en grupos vulnerables.\n");
            hay = 1;
        }
    }
    if (!hay) printf("  Sin alertas: todas las zonas dentro de limites.\n");
}

/* ---- Programa principal ---- */
int main(void) {
    Zona z[ZONAS];   /* arreglo de estructuras (matriz de datos) */
    int op;
    inicializar(z);
    do {
        printf("\n==================================\n");
        printf(" SIGPCA - Contaminacion del Aire\n");
        printf("==================================\n");
        printf(" 1. Monitorear niveles actuales\n");
        printf(" 2. Promedios historicos (30 dias)\n");
        printf(" 3. Predecir niveles a 24 horas\n");
        printf(" 4. Generar alertas preventivas\n");
        printf(" 0. Salir\n");
        printf("Seleccione una opcion: ");
        if (scanf("%d", &op) != 1) {            /* robustez ante texto */
            while (getchar() != '\n');
            op = -1;
        }
        switch (op) {
            case 1: monitoreo(z);  break;
            case 2: promedios(z);  break;
            case 3: prediccion(z); break;
            case 4: alertas(z);    break;
            case 0: printf("Saliendo del sistema...\n"); break;
            default: printf("Opcion invalida. Intente de nuevo.\n");
        }
    } while (op != 0);
    return 0;
}