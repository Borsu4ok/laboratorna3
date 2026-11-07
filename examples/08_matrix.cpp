#include <iostream>
#include <iomanip>
using namespace std;

const int N = 3;

void multiply(const double a[], const double b[], double result[], int n) {
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            double s = 0;
            for (int k = 0; k < n; k++)
                s += a[i * n + k] * b[k * n + j];
            result[i * n + j] = s;
        }
}

void printMatrix(const double m[], int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++)
            cout << setw(8) << m[i * n + j];
        cout << endl;
    }
}

double trace(const double m[], int n) {
    double t = 0;
    for (int i = 0; i < n; i++)
        t += m[i * n + i];
    return t;
}

int main() {
    double a[N * N] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    double identity[N * N] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    double b[N * N] = {0.5, -1, 2, 1.5, 0, -2, 3, 1, 0.25};
    double r[N * N];
    cout << fixed << setprecision(2);
    multiply(a, identity, r, N);
    cout << "A * I =" << endl;
    printMatrix(r, N);
    multiply(a, b, r, N);
    cout << "A * B =" << endl;
    printMatrix(r, N);
    cout << "trace(A * B) = " << setprecision(3) << trace(r, N) << endl;
    return 0;
}
