#pragma once
// Анализ и синтез на логически схеми – конзолно приложение (C++17)
// Операции: инверсия (!, ~, '), конюнкция (*, ., &, или слято записване), дезюнкция (+, |)
#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace std;

// ===================== Дърво на израза =====================
enum class T { VAR, CNST, NOT, AND, OR };
struct Node { T t = T::CNST; char v = 0; bool c = false; vector<Node> k; };

static bool g_over = false;                 // стил на инверсията: false = !a, true = a̅
static const string MARK = "\xCC\x85";      // комбиниращ надчерт U+0305

Node mkVar(char v) { Node n; n.t = T::VAR; n.v = v; return n; }
Node mkConst(bool c) { Node n; n.t = T::CNST; n.c = c; return n; }
Node mkNot(const Node& x) { Node n; n.t = T::NOT; n.k.push_back(x); return n; }
Node mkN(T t, vector<Node> ks) {
    if (ks.empty()) return mkConst(t == T::AND);
    if (ks.size() == 1) return ks[0];
    Node n; n.t = t; n.k = std::move(ks); return n;
}
Node neg(const Node& n) {
    if (n.t == T::NOT) return n.k[0];
    if (n.t == T::CNST) return mkConst(!n.c);
    return mkNot(n);
}
T other(T op) { return op == T::AND ? T::OR : T::AND; }

string str(const Node& n) {
    switch (n.t) {
    case T::VAR: return string(1, n.v);
    case T::CNST: return n.c ? "1" : "0";
    case T::NOT: {
        const Node& x = n.k[0];
        bool simple = x.t == T::VAR || x.t == T::NOT || x.t == T::CNST;
        string s = str(x);
        if (g_over) return (simple ? s : "(" + s + ")") + MARK;
        return "!" + (simple ? s : "(" + s + ")");
    }
    case T::AND: {
        string s;
        for (size_t i = 0; i < n.k.size(); ++i) {
            if (i) s += "*";
            bool w = n.k[i].t == T::OR || n.k[i].t == T::AND;
            s += w ? "(" + str(n.k[i]) + ")" : str(n.k[i]);
        }
        return s;
    }
    case T::OR: {
        string s;
        for (size_t i = 0; i < n.k.size(); ++i) {
            if (i) s += "+";
            s += n.k[i].t == T::OR ? "(" + str(n.k[i]) + ")" : str(n.k[i]);
        }
        return s;
    }
    }
    return "";
}
string P(const Node& n) {  // със скоби, ако е съставен
    return (n.t == T::AND || n.t == T::OR) ? "(" + str(n) + ")" : str(n);
}
// каноничен ключ (комутативно сравнение)
string key(const Node& n) {
    switch (n.t) {
    case T::VAR: return string(1, n.v);
    case T::CNST: return n.c ? "1" : "0";
    case T::NOT: return "~" + key(n.k[0]);
    default: {
        vector<string> ks;
        for (auto& x : n.k) ks.push_back(key(x));
        sort(ks.begin(), ks.end());
        string s = n.t == T::AND ? "&(" : "|(";
        for (auto& x : ks) s += x + ",";
        return s + ")";
    }
    }
}
void collectVars(const Node& n, set<char>& s) {
    if (n.t == T::VAR) s.insert(n.v);
    for (auto& x : n.k) collectVars(x, s);
}
bool eval(const Node& n, const bool* a) {
    switch (n.t) {
    case T::VAR: return a[(int)n.v];
    case T::CNST: return n.c;
    case T::NOT: return !eval(n.k[0], a);
    case T::AND: for (auto& x : n.k) if (!eval(x, a)) return false; return true;
    case T::OR: for (auto& x : n.k) if (eval(x, a)) return true; return false;
    }
    return false;
}

// ===================== Парсер =====================
struct Parser {
    string s; size_t p = 0;
    void ws() { while (p < s.size() && isspace((unsigned char)s[p])) ++p; }
    bool startsFactor() {
        ws();
        if (p >= s.size()) return false;
        char c = s[p];
        return isalpha((unsigned char)c) || c == '(' || c == '!' || c == '~' || c == '0' || c == '1';
    }
    Node expr() {
        vector<Node> ks{term()};
        for (;;) { ws(); if (p < s.size() && (s[p] == '+' || s[p] == '|')) { ++p; ks.push_back(term()); } else break; }
        return mkN(T::OR, ks);
    }
    Node term() {
        vector<Node> ks{factor()};
        for (;;) {
            ws();
            if (p < s.size() && (s[p] == '*' || s[p] == '.' || s[p] == '&')) { ++p; ks.push_back(factor()); }
            else if (startsFactor()) ks.push_back(factor());
            else break;
        }
        return mkN(T::AND, ks);
    }
    Node factor() {
        int ng = 0; ws();
        while (p < s.size() && (s[p] == '!' || s[p] == '~')) { ++ng; ++p; ws(); }
        Node a = atom();
        for (;;) {
            if (p < s.size() && s[p] == '\'') { ++ng; ++p; }
            else if (p + 1 < s.size() && (unsigned char)s[p] == 0xCC && (unsigned char)s[p + 1] == 0x85) { ++ng; p += 2; }
            else break;
        }
        while (ng--) a = mkNot(a);
        return a;
    }
    Node atom() {
        ws();
        if (p >= s.size()) throw runtime_error("Неочакван край на израза.");
        char c = s[p];
        if (isalpha((unsigned char)c)) { ++p; return mkVar(c); }
        if (c == '0' || c == '1') { ++p; return mkConst(c == '1'); }
        if (c == '(') {
            ++p; Node e = expr(); ws();
            if (p >= s.size() || s[p] != ')') throw runtime_error("Липсва затваряща скоба.");
            ++p; return e;
        }
        throw runtime_error(string("Неочакван символ '") + c + "'.");
    }
};
Node parse(const string& text) {
    Parser ps; ps.s = text; Node e = ps.expr(); ps.ws();
    if (ps.p != ps.s.size()) throw runtime_error(string("Неочакван символ '") + ps.s[ps.p] + "'.");
    return e;
}

// ===================== Правила (закони) =====================
using Rule = function<bool(Node&, string&)>;
string pairStr(T op, const Node& a, const Node& b) { return str(mkN(op, {a, b})); }
vector<Node> factorsOf(const Node& n, T inner) { return n.t == inner ? n.k : vector<Node>{n}; }
set<string> keySet(const vector<Node>& v) { set<string> s; for (auto& x : v) s.insert(key(x)); return s; }
string commPrefix(size_t i, size_t j) {
    return (i + 1 == j || j + 1 == i) ? "" : "Комутативен закон (пренареждане на членовете), после ";
}

bool rDNeg(Node& n, string& law) {
    if (n.t == T::NOT && n.k[0].t == T::NOT) {
        string b = str(n); Node t = n.k[0].k[0]; n = t;
        law = "Закон за двойното отрицание: " + b + " = " + str(n); return true;
    }
    return false;
}
bool rNotConst(Node& n, string& law) {
    if (n.t == T::NOT && n.k[0].t == T::CNST) {
        string b = str(n); Node t = mkConst(!n.k[0].c); n = t;
        law = "Отрицание на константа: " + b + " = " + str(n); return true;
    }
    return false;
}
bool rDeMorgan(Node& n, string& law) {
    if (n.t == T::NOT && (n.k[0].t == T::AND || n.k[0].t == T::OR)) {
        string b = str(n); T d = other(n.k[0].t); vector<Node> ks;
        for (auto& x : n.k[0].k) ks.push_back(neg(x));
        n = mkN(d, ks);
        law = "Теорема на Де Морган: " + b + " = " + str(n); return true;
    }
    return false;
}
Rule rFlatten(T op) {
    return [op](Node& n, string& law) {
        if (n.t != op) return false;
        bool any = false; for (auto& x : n.k) if (x.t == op) any = true;
        if (!any) return false;
        string b = str(n); vector<Node> ks;
        for (auto& x : n.k) { if (x.t == op) for (auto& y : x.k) ks.push_back(y); else ks.push_back(x); }
        n = mkN(op, ks);
        law = "Асоциативен закон: " + b + " = " + str(n); return true;
    };
}
Rule rConst(T op) {
    return [op](Node& n, string& law) {
        if (n.t != op) return false;
        bool ident = (op == T::AND), absb = !ident, hasAbs = false, hasId = false;
        for (auto& x : n.k) if (x.t == T::CNST) { if (x.c == absb) hasAbs = true; else hasId = true; }
        if (!hasAbs && !hasId) return false;
        string b = str(n);
        if (hasAbs) n = mkConst(absb);
        else { vector<Node> ks; for (auto& x : n.k) if (x.t != T::CNST) ks.push_back(x); n = mkN(op, ks); }
        law = string(op == T::AND ? "Закони x*0=0 и x*1=x: " : "Закони x+1=1 и x+0=x: ") + b + " = " + str(n);
        return true;
    };
}
Rule rCompl(T op) {
    return [op](Node& n, string& law) {
        if (n.t != op) return false;
        bool absb = (op == T::OR);
        for (size_t i = 0; i < n.k.size(); ++i)
            for (size_t j = i + 1; j < n.k.size(); ++j)
                if (key(n.k[i]) == key(neg(n.k[j]))) {
                    law = commPrefix(i, j) + "Закон за допълнението: " + pairStr(op, n.k[i], n.k[j]) + " = " + (absb ? "1" : "0");
                    n = mkConst(absb); return true;
                }
        return false;
    };
}
Rule rIdem(T op) {
    return [op](Node& n, string& law) {
        if (n.t != op) return false;
        for (size_t i = 0; i < n.k.size(); ++i)
            for (size_t j = i + 1; j < n.k.size(); ++j)
                if (key(n.k[i]) == key(n.k[j])) {
                    law = commPrefix(i, j) + "Закон за идемпотентността: " + pairStr(op, n.k[i], n.k[j]) + " = " + str(n.k[i]);
                    n.k.erase(n.k.begin() + j); n = mkN(op, n.k); return true;
                }
        return false;
    };
}
Rule rAbsorb(T op) {
    return [op](Node& n, string& law) {
        if (n.t != op) return false;
        T in = other(op);
        for (size_t i = 0; i < n.k.size(); ++i)
            for (size_t j = 0; j < n.k.size(); ++j) {
                if (i == j || n.k[j].t != in) continue;
                set<string> fa = keySet(factorsOf(n.k[i], in)), fb = keySet(n.k[j].k);
                if (fa.size() < fb.size() && includes(fb.begin(), fb.end(), fa.begin(), fa.end())) {
                    law = commPrefix(i, j) + "Закон за поглъщане: " + pairStr(op, n.k[i], n.k[j]) + " = " + str(n.k[i]);
                    n.k.erase(n.k.begin() + j); n = mkN(op, n.k); return true;
                }
            }
        return false;
    };
}
Rule rAdhesion(T op) {
    return [op](Node& n, string& law) {
        if (n.t != op) return false;
        T in = other(op);
        for (size_t i = 0; i < n.k.size(); ++i)
            for (size_t j = i + 1; j < n.k.size(); ++j) {
                const Node &a = n.k[i], &b = n.k[j];
                if (a.t != in || b.t != in || a.k.size() != b.k.size()) continue;
                set<string> sa = keySet(a.k), sb = keySet(b.k);
                vector<string> da, db;
                for (auto& x : sa) if (!sb.count(x)) da.push_back(x);
                for (auto& x : sb) if (!sa.count(x)) db.push_back(x);
                if (da.size() != 1 || db.size() != 1) continue;
                Node p; vector<Node> common;
                for (auto& x : a.k) { if (key(x) == da[0]) p = x; else common.push_back(x); }
                if (key(neg(p)) != db[0]) continue;
                Node S = mkN(in, common);
                string pr = pairStr(op, a, b);
                string mid = (op == T::OR) ? P(S) + "*(" + str(p) + "+" + str(neg(p)) + ")"
                                           : P(S) + "+" + str(p) + "*" + str(neg(p));
                law = commPrefix(i, j) + "Закон за слепването: " + pr + " = " + mid + " = " + str(S);
                n.k[i] = S; n.k.erase(n.k.begin() + j); n = mkN(op, n.k); return true;
            }
        return false;
    };
}
// a + !a*b = a + b  /  a*(!a + b) = a*b
Rule rGenAbs(T op) {
    return [op](Node& n, string& law) {
        if (n.t != op) return false;
        T in = other(op);
        for (size_t i = 0; i < n.k.size(); ++i) {
            if (n.k[i].t == T::CNST) continue;
            string nk = key(neg(n.k[i]));
            for (size_t j = 0; j < n.k.size(); ++j) {
                if (i == j || n.k[j].t != in || n.k[j].k.size() < 2) continue;
                vector<Node> rest; bool found = false;
                for (auto& x : n.k[j].k) { if (!found && key(x) == nk) found = true; else rest.push_back(x); }
                if (!found) continue;
                const Node a = n.k[i]; Node NA = neg(a), R = mkN(in, rest);
                string A = str(a), NAs = str(NA);
                if (op == T::OR)
                    law = commPrefix(i, j) + "Дистрибутивен закон и закон за допълнението:\n        " + A + "+" + NAs + "*" + P(R) +
                          " = (" + A + "+" + NAs + ")(" + A + "+" + str(R) + ") = 1*(" + A + "+" + str(R) + ") = " + A + "+" + str(R);
                else
                    law = commPrefix(i, j) + "Дистрибутивен закон и закон за допълнението:\n        " + P(a) + "*(" + NAs + "+" + str(R) +
                          ") = " + P(a) + "*" + P(NA) + "+" + P(a) + "*" + P(R) + " = 0+" + P(a) + "*" + P(R) + " = " + P(a) + "*" + P(R);
                n.k[j] = R; return true;
            }
        }
        return false;
    };
}
Rule rFactor(T op) {
    return [op](Node& n, string& law) {
        if (n.t != op) return false;
        T in = other(op);
        for (size_t i = 0; i < n.k.size(); ++i) {
            if (n.k[i].t != in) continue;
            for (auto& f : n.k[i].k) {
                string kf = key(f); vector<size_t> G;
                for (size_t j = 0; j < n.k.size(); ++j)
                    if (n.k[j].t == in && keySet(n.k[j].k).count(kf)) G.push_back(j);
                if (G.size() < 2) continue;
                vector<Node> rests;
                for (size_t j : G) {
                    vector<Node> o; bool skip = false;
                    for (auto& x : n.k[j].k) { if (!skip && key(x) == kf) skip = true; else o.push_back(x); }
                    rests.push_back(mkN(in, o));
                }
                Node nw = mkN(in, {f, mkN(op, rests)});
                string b = str(n); vector<Node> ks; bool put = false;
                for (size_t j = 0; j < n.k.size(); ++j) {
                    if (find(G.begin(), G.end(), j) != G.end()) { if (!put) { ks.push_back(nw); put = true; } }
                    else ks.push_back(n.k[j]);
                }
                n = mkN(op, ks);
                law = "Дистрибутивен закон (общ множител пред скоби): " + b + " = " + str(n); return true;
            }
        }
        return false;
    };
}
vector<Rule> rulesNNF() { return {rDNeg, rNotConst, rDeMorgan}; }
vector<Rule> rulesAll() {
    return {rDNeg, rNotConst, rDeMorgan, rFlatten(T::AND), rFlatten(T::OR), rConst(T::AND), rConst(T::OR),
            rCompl(T::AND), rCompl(T::OR), rIdem(T::AND), rIdem(T::OR), rAbsorb(T::OR), rAbsorb(T::AND),
            rAdhesion(T::OR), rAdhesion(T::AND), rGenAbs(T::OR), rGenAbs(T::AND), rFactor(T::OR), rFactor(T::AND)};
}
bool applyOnce(Node& n, const Rule& r, string& law) {
    if (r(n, law)) return true;
    for (auto& c : n.k) if (applyOnce(c, r, law)) return true;
    return false;
}
Node runRules(Node e, const vector<Rule>& rules, int& step, bool print = true) {
    for (int g = 0; g < 300; ++g) {
        bool did = false;
        for (auto& r : rules) {
            string law;
            if (applyOnce(e, r, law)) {
                ++step; did = true;
                if (print) cout << "  Стъпка " << step << ": " << law << "\n      => " << str(e) << "\n";
                break;
            }
        }
        if (!did) break;
    }
    return e;
}

// ===================== Помощни за вход/изход =====================
string trim(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == string::npos ? "" : s.substr(a, b - a + 1);
}
static function<bool(const string&, string&)> g_reader;  // GUI подменя четенето на вход
bool readLine(const string& prompt, string& out) {
    if (g_reader) return g_reader(prompt, out);
    cout << prompt << flush;
    if (!getline(cin, out)) return false;
    out = trim(out); return true;
}
size_t dispLen(const string& s) {
    size_t n = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = s[i];
        if ((c & 0xC0) == 0x80) continue;
        if (c == 0xCC && i + 1 < s.size() && (unsigned char)s[i + 1] == 0x85) { ++i; continue; }
        ++n;
    }
    return n;
}
string pad(const string& s, size_t w) { size_t l = dispLen(s); return s + string(l < w ? w - l : 0, ' '); }
string join(const vector<int>& v) { string s; for (size_t i = 0; i < v.size(); ++i) s += (i ? "," : "") + to_string(v[i]); return s; }

// ===================== Тип 1: свързване на схема =====================
struct Circ { vector<string> g; set<string> seen; };
string build(const Node& n, Circ& c) {
    switch (n.t) {
    case T::VAR: case T::CNST: return str(n);
    case T::NOT: {
        string x = build(n.k[0], c), out = str(n);
        if (c.seen.insert(out).second) c.g.push_back("НЕ  (" + x + ")  =>  " + out);
        return out;
    }
    default: {
        vector<Node> pref{n.k[0]}; string cur = build(n.k[0], c);
        for (size_t i = 1; i < n.k.size(); ++i) {
            string r = build(n.k[i], c); pref.push_back(n.k[i]);
            string out = str(mkN(n.t, pref));
            if (c.seen.insert(out).second)
                c.g.push_back(string(n.t == T::AND ? "И   (" : "ИЛИ (") + cur + " , " + r + ")  =>  " + out);
            cur = out;
        }
        return cur;
    }
    }
}
void tree(const Node& n, const string& pre, bool last, bool root) {
    string name;
    switch (n.t) {
    case T::VAR: name = string("вход ") + n.v; break;
    case T::CNST: name = string("константа ") + (n.c ? "1" : "0"); break;
    case T::NOT: name = "НЕ"; break; case T::AND: name = "И"; break; case T::OR: name = "ИЛИ"; break;
    }
    cout << pre << (root ? "" : (last ? "└─ " : "├─ ")) << name << "\n";
    string np = pre + (root ? "" : (last ? "   " : "│  "));
    for (size_t i = 0; i < n.k.size(); ++i) tree(n.k[i], np, i + 1 == n.k.size(), false);
}
void taskCircuit() {
    string s; if (!readLine("Функция (напр. !a*b+c): ", s) || s.empty()) return;
    Node f = parse(s); set<char> vs; collectVars(f, vs);
    cout << "\nf = " << str(f) << "\nВходове:"; for (char c : vs) cout << " " << c; cout << "\n\n";
    Circ c; string out = build(f, c);
    cout << "Свързване на елементите (всеки елемент е с най-много 2 входа):\n";
    for (size_t i = 0; i < c.g.size(); ++i) cout << "  Елемент " << i + 1 << ": " << c.g[i] << "\n";
    cout << "Изход: f = " << out << "\n\nСхема (изходът е най-отгоре, входовете са листата):\n";
    tree(f, "  ", true, true);
}

// ===================== Тип 2: алгебрични преобразувания =====================
void taskAlgebra() {
    string s; if (!readLine("Израз (напр. !(a+b)*c+a*b): ", s) || s.empty()) return;
    Node f = parse(s); int step = 0;
    cout << "\nДадено: " << str(f) << "\n";
    Node r = runRules(f, rulesAll(), step);
    if (!step) cout << "  Изразът не може да се опрости допълнително.\n";
    cout << "Резултат: " << str(r) << "\n";
}
void taskIdentity() {
    string s; if (!readLine("Тъждество (напр. a+!a*b=a+b): ", s) || s.empty()) return;
    size_t eq = s.find('='); if (eq == string::npos) throw runtime_error("Липсва знак '='.");
    Node L = parse(s.substr(0, eq)), R = parse(s.substr(eq + 1));
    int st = 0;
    cout << "\nДоказваме: " << str(L) << " = " << str(R) << "\n\nЛява страна: " << str(L) << "\n";
    Node l2 = runRules(L, rulesAll(), st); if (!st) cout << "  (не се опростява)\n";
    st = 0; cout << "\nДясна страна: " << str(R) << "\n";
    Node r2 = runRules(R, rulesAll(), st); if (!st) cout << "  (не се опростява)\n";
    cout << "\nЛява страна => " << str(l2) << "\nДясна страна => " << str(r2) << "\n";
    if (key(l2) == key(r2)) { cout << "Двете страни се свеждат до един и същ израз  =>  тъждеството е ДОКАЗАНО.\n"; return; }
    set<char> vs; collectVars(L, vs); collectVars(R, vs);
    bool a[128] = {}, same = true; vector<char> v(vs.begin(), vs.end());
    for (int m = 0; m < (1 << v.size()); ++m) {
        for (size_t i = 0; i < v.size(); ++i) a[(int)v[i]] = (m >> i) & 1;
        if (eval(L, a) != eval(R, a)) same = false;
    }
    cout << (same ? "Страните са еквивалентни (проверка с таблица), но не се свеждат алгебрично до една и съща форма с наличните закони.\n"
                  : "Тъждеството НЕ е вярно – има набор от стойности, при които страните са различни.\n");
}

// ===================== Тип 3: таблица на истината, минтерми, макстерми =====================
using Cl = pair<uint32_t, uint32_t>;  // (положителни, отрицателни) литерали по индекс на променлива
string litStr(char v, bool ng) { return str(ng ? mkNot(mkVar(v)) : mkVar(v)); }
vector<string> lits(const Cl& c, const vector<char>& vars) {
    vector<string> r;
    for (size_t i = 0; i < vars.size(); ++i) {
        if ((c.first >> i) & 1) r.push_back(litStr(vars[i], false));
        if ((c.second >> i) & 1) r.push_back(litStr(vars[i], true));
    }
    return r;
}
string joinS(const vector<string>& v, const string& sep) { string s; for (size_t i = 0; i < v.size(); ++i) s += (i ? sep : "") + v[i]; return s; }
string clStr(const Cl& c, const vector<char>& vars, bool cnf) {
    auto l = lits(c, vars);
    if (l.empty()) return cnf ? "0" : "1";
    return cnf ? "(" + joinS(l, "+") + ")" : joinS(l, "*");
}
string formStr(const vector<Cl>& f, const vector<char>& vars, bool cnf) {
    if (f.empty()) return cnf ? "1" : "0";
    string s;
    for (size_t i = 0; i < f.size(); ++i) s += (i && !cnf ? "+" : "") + clStr(f[i], vars, cnf);
    return s;
}
vector<Cl> cross(const vector<Cl>& A, const vector<Cl>& B) {
    vector<Cl> r;
    for (auto& a : A) for (auto& b : B) r.push_back({a.first | b.first, a.second | b.second});
    return r;
}
int vidx(const vector<char>& v, char c) { return find(v.begin(), v.end(), c) - v.begin(); }
vector<Cl> forms(const Node& n, bool cnf, const vector<char>& vars) {
    T outer = cnf ? T::AND : T::OR;
    switch (n.t) {
    case T::VAR: return {{1u << vidx(vars, n.v), 0}};
    case T::NOT: return {{0, 1u << vidx(vars, n.k[0].v)}};
    case T::CNST: return (cnf ? !n.c : n.c) ? vector<Cl>{{0, 0}} : vector<Cl>{};
    default: break;
    }
    if (n.t == outer) { vector<Cl> r; for (auto& x : n.k) { auto f = forms(x, cnf, vars); r.insert(r.end(), f.begin(), f.end()); } return r; }
    vector<Cl> acc{{0, 0}};
    for (auto& x : n.k) acc = cross(acc, forms(x, cnf, vars));
    return acc;
}
vector<Cl> cleanForm(const vector<Cl>& f) {
    vector<Cl> r; set<Cl> seen;
    for (auto& c : f) if (!(c.first & c.second) && seen.insert(c).second) r.push_back(c);
    return r;
}
int codeOf(const Cl& c, int n, bool cnf) {  // пълен клауз/терм -> номер
    int m = 0;
    for (int i = 0; i < n; ++i) m = m * 2 + (((cnf ? c.second : c.first) >> i) & 1);
    return m;
}
string bits(int m, int n) { string s; for (int i = n - 1; i >= 0; --i) s += ((m >> i) & 1) ? '1' : '0'; return s; }

void algSolve(const Node& f, const vector<char>& vars, bool cnf, const vector<int>& val) {
    int n = vars.size(), step = 0;
    cout << "\n--- Алгебрично решение (" << (cnf ? "СКНФ – произведение от макстерми" : "СДНФ – сума от минтерми") << ") ---\n";
    cout << "  f = " << str(f) << "\n";
    Node e = runRules(f, rulesNNF(), step);
    auto raw = forms(e, cnf, vars);
    cout << "  Прилагаме дистрибутивния закон (" << (cnf ? "дезюнкцията спрямо конюнкцията" : "конюнкцията спрямо дезюнкцията")
         << "):\n      f = " << formStr(raw, vars, cnf) << "\n";
    auto cl = cleanForm(raw);
    if (cl.size() != raw.size()) {
        cout << "  Махаме " << (cnf ? "членове със x+x̄=1" : "членове със x*x̄=0") << " и повтарящите се (закон за идемпотентността):\n      f = " << formStr(cl, vars, cnf) << "\n";
    }
    vector<Cl> full;
    bool anyExp = false;
    for (auto& c : cl) {
        vector<int> miss; for (int i = 0; i < n; ++i) if (!(((c.first | c.second) >> i) & 1)) miss.push_back(i);
        if (miss.empty()) { full.push_back(c); continue; }
        anyExp = true;
        auto l = lits(c, vars); string mid;
        for (int i : miss) {
            string v(1, vars[i]), nv = litStr(vars[i], true);
            l.push_back(cnf ? v + "*" + nv : "(" + v + "+" + nv + ")");
        }
        mid = cnf ? "(" + joinS(l, "+") + ")" : joinS(l, "*");
        vector<Cl> ex;
        for (int mask = 0; mask < (1 << miss.size()); ++mask) {
            Cl d = c;
            for (size_t j = 0; j < miss.size(); ++j) ((mask >> (miss.size() - 1 - j)) & 1 ? d.second : d.first) |= 1u << miss[j];
            ex.push_back(d); full.push_back(d);
        }
        cout << "  " << clStr(c, vars, cnf) << " = " << mid << " = " << formStr(ex, vars, cnf) << "\n";
    }
    if (anyExp) cout << "  (допълваме липсващите променливи със закона за допълнението и дистрибутивния закон)\n";
    auto fin = cleanForm(full);
    if (anyExp) cout << "  Събираме:\n      f = " << formStr(full, vars, cnf) << "\n";
    if (fin.size() != full.size()) cout << "  Махаме повторенията (идемпотентен закон):\n      f = " << formStr(fin, vars, cnf) << "\n";
    cout << "  Двоични кодове (" << (cnf ? "x̄→1, x→0" : "x→1, x̄→0") << "):\n";
    vector<int> nums;
    for (auto& c : fin) { int m = codeOf(c, n, cnf); nums.push_back(m); cout << "      " << pad(clStr(c, vars, cnf), 3 * n + 4) << " -> " << bits(m, n) << " = " << m << "\n"; }
    sort(nums.begin(), nums.end());
    cout << "  f = " << (cnf ? "ΠM(" : "Σm(") << join(nums) << ")" << (nums.empty() ? (cnf ? "   (празно произведение = 1)" : "   (празна сума = 0)") : "") << "\n";
    vector<int> chk; for (int m = 0; m < (1 << n); ++m) if (val[m] == (cnf ? 0 : 1)) chk.push_back(m);
    cout << "  Проверка с таблицата на истината: " << (chk == nums ? "съвпада ✓" : "НЕ съвпада ✗") << "\n";
}
void taskTable() {
    string s; if (!readLine("Функция (напр. f(a,b,c)=!a*b+!a*c): ", s) || s.empty()) return;
    vector<char> vars; string body = s; size_t eq = s.find('=');
    if (eq != string::npos) {
        string head = s.substr(0, eq); body = s.substr(eq + 1);
        size_t a = head.find('('), b = head.find(')');
        if (a != string::npos && b != string::npos) for (size_t i = a + 1; i < b; ++i) if (isalpha((unsigned char)head[i])) vars.push_back(head[i]);
    }
    Node f = parse(body); set<char> vs; collectVars(f, vs);
    for (char c : vs) if (find(vars.begin(), vars.end(), c) == vars.end()) vars.push_back(c);
    if (vars.empty()) { cout << "Функцията е константа: " << str(f) << "\n"; return; }
    if (vars.size() > 8) throw runtime_error("Поддържат се най-много 8 променливи.");
    if (eq == string::npos) sort(vars.begin(), vars.end());
    int n = vars.size(); vector<int> val(1 << n); bool a[128] = {};
    for (int m = 0; m < (1 << n); ++m) {
        for (int i = 0; i < n; ++i) a[(int)vars[i]] = (m >> (n - 1 - i)) & 1;
        val[m] = eval(f, a);
    }
    cout << "\nf = " << str(f) << "\n\nТаблица на истината, минтерми и макстерми:\n";
    string vh; for (char c : vars) { vh += c; vh += ' '; }
    size_t wc = 3 * n + 1;
    cout << "  " << pad("№", 3) << pad(vh, 2 * n + 1) << pad("f", 2) << pad("минтерм m", wc + 2) << "макстерм M\n";
    vector<int> ones, zeros;
    for (int m = 0; m < (1 << n); ++m) {
        string row; vector<string> mn, mx;
        for (int i = 0; i < n; ++i) {
            bool bt = (m >> (n - 1 - i)) & 1; row += string(1, bt ? '1' : '0') + ' ';
            mn.push_back(litStr(vars[i], !bt)); mx.push_back(litStr(vars[i], bt));
        }
        (val[m] ? ones : zeros).push_back(m);
        cout << "  " << pad(to_string(m), 3) << pad(row, 2 * n + 1) << pad(to_string(val[m]), 2)
             << pad(joinS(mn, "*"), wc + 2) << "(" << joinS(mx, "+") << ")\n";
    }
    cout << "\nЕдиници (f=1) на набори №: " << (ones.empty() ? "няма" : join(ones));
    cout << "\nНули    (f=0) на набори №: " << (zeros.empty() ? "няма" : join(zeros)) << "\n";
    cout << "\nСДНФ: f = Σm(" << join(ones) << ")";
    { vector<string> t; for (int m : ones) { vector<string> l; for (int i = 0; i < n; ++i) l.push_back(litStr(vars[i], !((m >> (n - 1 - i)) & 1))); t.push_back(joinS(l, "*")); }
      cout << (t.empty() ? " = 0" : " = " + joinS(t, " + ")) << "\n"; }
    cout << "СКНФ: f = ΠM(" << join(zeros) << ")";
    { string t; for (int m : zeros) { vector<string> l; for (int i = 0; i < n; ++i) l.push_back(litStr(vars[i], (m >> (n - 1 - i)) & 1)); t += "(" + joinS(l, "+") + ")"; }
      cout << (t.empty() ? " = 1" : " = " + t) << "\n"; }
    algSolve(f, vars, true, val);
    algSolve(f, vars, false, val);
}

