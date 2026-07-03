/* SIGPCA - Sistema Integral de Gestion y Prediccion de Contaminacion
   del Aire en Zonas Urbanas (Quito).  Programacion 1 - ISWZ1102.
   Un solo archivo. Usa funciones, punteros, arreglos, matrices y
   estructuras. Sin memoria dinamica, sin librerias externas.
   Los datos los ingresa el usuario con validacion de entradas. */
#include <stdio.h>
#include <string.h>

#define ZONAS   5      /* maximo de zonas monitoreadas        */
#define DIAS    30     /* maximo de dias de historico por zona*/
#define VENTANA 7      /* dias del promedio movil ponderado   */

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

/* ============ FUNCIONES DE VALIDACION DE ENTRADAS ============ */

/* limpia lo que quede en el buffer de entrada hasta el fin de linea */
void limpiarBuffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { /* descarta */ }
}

/* Lee un entero en [min, max]. Rechaza letras y valores fuera de rango. */
int leerEntero(const char *msg, int min, int max) {
    int valor;
    for (;;) {
        printf("%s", msg);
        if (scanf("%d", &valor) != 1) {         /* no es un numero */
            limpiarBuffer();
            printf("  [Error] Ingrese un numero entero valido.\n");
            continue;
        }
        limpiarBuffer();                         /* descarta sobrantes */
        if (valor < min || valor > max) {
            printf("  [Error] El valor debe estar entre %d y %d.\n", min, max);
            continue;
        }
        return valor;
    }
}

/* Lee un real en [min, max]. Rechaza letras, negativos (si min>=0)
   y valores fuera de rango. */
float leerReal(const char *msg, float min, float max) {
    float valor;
    for (;;) {
        printf("%s", msg);
        if (scanf("%f", &valor) != 1) {          /* no es un numero */
            limpiarBuffer();
            printf("  [Error] Ingrese un numero valido (use punto decimal).\n");
            continue;
        }
        limpiarBuffer();
        if (valor < min || valor > max) {
            printf("  [Error] El valor debe estar entre %.1f y %.1f.\n", min, max);
            continue;
        }
        return valor;
    }
}

/* Lee una linea de texto no vacia para el nombre de la zona */
void leerTexto(const char *msg, char *dest, int tam) {
    printf("%s", msg);
    if (fgets(dest, tam, stdin) != NULL) {
        size_t n = strlen(dest);
        if (n > 0 && dest[n - 1] == '\n') dest[n - 1] = '\0';
    }
    if (dest[0] == '\0') strcpy(dest, "SinNombre");
}

/* ============ INGRESO DE DATOS (POR EL USUARIO) ============ */
void ingresarDatos(Zona z[], int nz, int nd) {
    int i, d;
    for (i = 0; i < nz; i++) {
        printf("\n----- ZONA %d de %d -----\n", i + 1, nz);
        leerTexto(" Nombre de la zona: ", z[i].nombre, 20);

        printf(" Factores climaticos actuales:\n");
        z[i].clima.temp    = leerReal("   Temperatura (0-45 C): ", 0.0f, 45.0f);
        z[i].clima.viento  = leerReal("   Viento (0-50 m/s): ", 0.0f, 50.0f);
        z[i].clima.humedad = leerReal("   Humedad (0-100 %): ", 0.0f, 100.0f);

        printf(" Registros diarios de contaminantes (%d dias):\n", nd);
        for (d = 0; d < nd; d++) {
            printf("  Dia %d:\n", d + 1);
            z[i].historico[d].pm25 = leerReal("   PM2.5 (ug/m3): ", 0.0f, 1000.0f);
            z[i].historico[d].no2  = leerReal("   NO2   (ug/m3): ", 0.0f, 1000.0f);
            z[i].historico[d].so2  = leerReal("   SO2   (ug/m3): ", 0.0f, 1000.0f);
            z[i].historico[d].co   = leerReal("   CO    (mg/m3): ", 0.0f, 100.0f);
        }
        z[i].actual = z[i].historico[nd - 1];    /* dia mas reciente */
    }
}

/* ============ CALCULOS ============ */
/* copia un contaminante (0=PM2.5,1=NO2,2=SO2,3=CO) a un arreglo */
void serie(const Zona *z, int campo, int nd, float out[]) {
    int d;
    for (d = 0; d < nd; d++) {
        const Contaminantes *c = &z->historico[d];
        out[d] = (campo==0)?c->pm25:(campo==1)?c->no2:(campo==2)?c->so2:c->co;
    }
}
/* promedio aritmetico simple */
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
void predecir(Zona *z, int nd) {
    float v[DIAS];
    serie(z,0,nd,v); z->prediccion.pm25 = pmp(v,nd,VENTANA)*factorClima(&z->clima,1);
    serie(z,1,nd,v); z->prediccion.no2  = pmp(v,nd,VENTANA)*factorClima(&z->clima,0);
    serie(z,2,nd,v); z->prediccion.so2  = pmp(v,nd,VENTANA)*factorClima(&z->clima,0);
    serie(z,3,nd,v); z->prediccion.co   = pmp(v,nd,VENTANA)*factorClima(&z->clima,0);
}
/* devuelve "OK" o "EXCEDE" segun el limite */
const char* estado(float valor, float limite) {
    return valor > limite ? "EXCEDE" : "OK";
}

/* ============ OPCIONES DEL MENU ============ */
void monitoreo(const Zona z[], int nz) {
    int i;
    printf("\nMONITOREO DE NIVELES ACTUALES (vs limites OMS)\n");
    for (i = 0; i < nz; i++) {
        const Contaminantes *a = &z[i].actual;
        printf("\nZona: %s\n", z[i].nombre);
        printf("  PM2.5 = %6.1f ug/m3  [%s]\n", a->pm25, estado(a->pm25,L_PM25));
        printf("  NO2   = %6.1f ug/m3  [%s]\n", a->no2,  estado(a->no2, L_NO2));
        printf("  SO2   = %6.1f ug/m3  [%s]\n", a->so2,  estado(a->so2, L_SO2));
        printf("  CO    = %6.2f mg/m3  [%s]\n", a->co,   estado(a->co,  L_CO));
    }
}
void promedios(const Zona z[], int nz, int nd) {
    int i; float v[DIAS];
    printf("\nPROMEDIO HISTORICO %d DIAS (vs limites OMS)\n", nd);
    for (i = 0; i < nz; i++) {
        printf("\nZona: %s\n", z[i].nombre);
        serie(&z[i],0,nd,v); printf("  PM2.5 = %6.1f  [%s]\n", promedio(v,nd), estado(promedio(v,nd),L_PM25));
        serie(&z[i],1,nd,v); printf("  NO2   = %6.1f  [%s]\n", promedio(v,nd), estado(promedio(v,nd),L_NO2));
        serie(&z[i],2,nd,v); printf("  SO2   = %6.1f  [%s]\n", promedio(v,nd), estado(promedio(v,nd),L_SO2));
        serie(&z[i],3,nd,v); printf("  CO    = %6.2f  [%s]\n", promedio(v,nd), estado(promedio(v,nd),L_CO));
    }
}
void prediccion(Zona z[], int nz, int nd) {
    int i;
    printf("\nPREDICCION A 24 HORAS (vs limites OMS)\n");
    for (i = 0; i < nz; i++) {
        predecir(&z[i], nd);
        const Contaminantes *p = &z[i].prediccion;
        printf("\nZona: %s  (T=%.0fC Viento=%.1fm/s Hum=%.0f%%)\n",
               z[i].nombre, z[i].clima.temp, z[i].clima.viento, z[i].clima.humedad);
        printf("  PM2.5 = %6.1f  [%s]\n", p->pm25, estado(p->pm25,L_PM25));
        printf("  NO2   = %6.1f  [%s]\n", p->no2,  estado(p->no2, L_NO2));
        printf("  SO2   = %6.1f  [%s]\n", p->so2,  estado(p->so2, L_SO2));
        printf("  CO    = %6.2f  [%s]\n", p->co,   estado(p->co,  L_CO));
    }
}
void alertas(Zona z[], int nz, int nd) {
    int i, hay = 0;
    printf("\nALERTAS PREVENTIVAS (segun prediccion 24 h)\n");
    for (i = 0; i < nz; i++) {
        predecir(&z[i], nd);
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

/* ============ PROGRAMA PRINCIPAL ============ */
int main(void) {
    Zona z[ZONAS];   /* arreglo de estructuras (matriz de datos) */
    int nz, nd, op;

    printf("==================================\n");
    printf(" SIGPCA - Contaminacion del Aire\n");
    printf("==================================\n");
    printf("Ingreso de datos (validado):\n");
    nz = leerEntero("Numero de zonas (1-5): ", 1, ZONAS);
    nd = leerEntero("Numero de dias de historico (1-30): ", 1, DIAS);
    ingresarDatos(z, nz, nd);

    do {
        printf("\n==================================\n");
        printf(" MENU PRINCIPAL\n");
        printf("==================================\n");
        printf(" 1. Monitorear niveles actuales\n");
        printf(" 2. Promedios historicos\n");
        printf(" 3. Predecir niveles a 24 horas\n");
        printf(" 4. Generar alertas preventivas\n");
        printf(" 0. Salir\n");
        op = leerEntero("Seleccione una opcion (0-4): ", 0, 4);
        switch (op) {
            case 1: monitoreo(z, nz);      break;
            case 2: promedios(z, nz, nd);  break;
            case 3: prediccion(z, nz, nd); break;
            case 4: alertas(z, nz, nd);    break;
            case 0: printf("Saliendo del sistema...\n"); break;
        }
    } while (op != 0);
    return 0;
}
