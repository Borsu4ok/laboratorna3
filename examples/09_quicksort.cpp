#include <iostream>
using namespace std;

const int SIZE = 12;
int numbers[SIZE] = {33, -5, 12, 0, 99, 12, 7, -40, 58, 21, 3, 64};
int comparisons = 0;

void swap(int& a, int& b) {
    int t = a;
    a = b;
    b = t;
}

int partition(int a[], int low, int high) {
    int pivot = a[high];
    int i = low - 1;
    for (int j = low; j < high; j++) {
        comparisons++;
        if (a[j] <= pivot)
            swap(a[++i], a[j]);
    }
    swap(a[i + 1], a[high]);
    return i + 1;
}

void quicksort(int a[], int low, int high) {
    if (low < high) {
        int p = partition(a, low, high);
        quicksort(a, low, p - 1);
        quicksort(a, p + 1, high);
    }
}

int binarySearch(const int a[], int n, int key) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (a[mid] == key)
            return mid;
        else if (a[mid] < key)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return -1;
}

int main() {
    quicksort(numbers, 0, SIZE - 1);
    for (auto value : numbers)
        cout << value << " ";
    cout << endl << "comparisons: " << comparisons << endl;

    int keys[] = {58, 1, -40, 99};
    for (int key : keys)
        cout << "binarySearch(" << key << ") -> " << binarySearch(numbers, SIZE, key) << endl;

    for (auto& value : numbers)
        value *= 2;
    auto total = 0;
    for (const int& value : numbers)
        total += value;
    cout << "doubled: first = " << numbers[0] << ", last = " << numbers[SIZE - 1] << ", total = " << total << endl;
    return 0;
}
