#include <iostream>
using namespace std;

const int N = 10;

void swap(int& a, int& b) {
    int tmp = a;
    a = b;
    b = tmp;
}

void printArray(const int a[], int n) {
    for (int i = 0; i < n; i++) {
        cout << a[i];
        if (i + 1 < n)
            cout << " ";
    }
    cout << endl;
}

void bubbleSort(int a[], int n) {
    bool swapped = true;
    int pass = 0;
    while (swapped) {
        swapped = false;
        for (int i = 0; i < n - 1 - pass; i++) {
            if (a[i] > a[i + 1]) {
                swap(a[i], a[i + 1]);
                swapped = true;
            }
        }
        pass++;
    }
}

int main() {
    int data[N] = {42, 7, 19, -3, 88, 0, 15, 7, 64, 23};
    cout << "before: ";
    printArray(data, N);
    bubbleSort(data, N);
    cout << "after:  ";
    printArray(data, N);

    int sum = 0;
    for (int x : data)
        sum += x;
    cout << "sum = " << sum << ", min = " << data[0] << ", max = " << data[N - 1] << endl;

    int x = 5, y = 9;
    swap(x, y);
    cout << "after swap(x, y): x = " << x << ", y = " << y << endl;
    return 0;
}
