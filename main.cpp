#include "core.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// ===================== Меню =====================
int main() {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif
    for (;;) {
        cout << "\n===== Анализ и синтез на логически схеми =====\n"
             << " 1. Свързване на логическа схема (И, ИЛИ, НЕ)\n"
             << " 2. Алгебрични преобразувания със закони (пълно решение)\n"
             << " 3. Доказване на тъждество (напр. a+!a*b=a+b)\n"
             << " 4. Функция: таблица на истината, минтерми, макстерми + алгебрично решение\n"
             << " 5. Смяна на стила на инверсията (сега: " << (g_over ? "a̅" : "!a") << ")\n"
             << " 0. Изход\n"
             << "Запис: инверсия ! ~ или ' (допуска се и !!!a); И: * . & или слято (ab); ИЛИ: + или |\n";
        string c; if (!readLine("Избор: ", c) || c == "0") break;
        try {
            if (c == "1") taskCircuit();
            else if (c == "2") taskAlgebra();
            else if (c == "3") taskIdentity();
            else if (c == "4") taskTable();
            else if (c == "5") g_over = !g_over;
            else cout << "Невалиден избор.\n";
        } catch (const exception& e) { cout << "Грешка: " << e.what() << "\n"; }
    }
    return 0;
}
