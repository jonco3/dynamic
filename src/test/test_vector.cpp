#include "src/vector.h"

#include "src/test.h"

#include <vector>

// Element class to test correct construction and destruction of vector
// elements.
struct Element
{
    static size_t Count;

    Element()
      : value(0), loc(this)
    {
        Count++;
    }

    Element(int v)
      : value(v), loc(this)
    {
        Count++;
    }

    Element(const Element& other)
      : value(other.value), loc(this)
    {
        Count++;
    }

    ~Element() {
        check();
        testNotEqual(Count, 0);
        Count--;
    }

    Element& operator=(const Element& other) {
        check();
        other.check();
        value = other.value;
        return *this;
    }

    operator int() const {
        check();
        return value;
    }

    bool operator==(int other) { return value == other; }
    bool operator!=(int other) { return value != other; }

  private:
    int value;
    const Element* loc;

    void check() const {
        testEqual(this, loc);
    }
};

size_t Element::Count = 0;

ostream& operator<<(ostream& s, const Element& e)
{
    s << int(e);
    return s;
}

template <typename V>
static void populate(V& v, size_t n)
{
    for (size_t i = v.size(); i < n; i++)
        v.push_back(int(i));
}

template <typename V>
static void checkContents(V& v, size_t n)
{
    for (size_t i = 0; i < n; i++)
        testEqual(v[i], int(i));
}

template <typename V>
static void testPush(int max)
{
    V v;
    testEqual(v.size(), 0);
    testTrue(v.empty());

    v.push_back(0);
    testEqual(v.size(), 1);
    testFalse(v.empty());
    testEqual(v[0], 0);

    populate(v, max);
    testEqual(v.size(), max);
    checkContents(v, max);
}

template <typename V>
static void testFill(size_t size, int value)
{
    V v(size, value);
    testEqual(v.size(), size);
    for (size_t i = 0; i < size; i++)
        testEqual(v[i], value);
}

template <typename V>
static void testResize()
{
    V v;
    testEqual(v.size(), 0);

    v.resize(10);
    testEqual(v.size(), 10);
    for (int i = 0; i < 10; i++)
        v[i] = i;

    v.resize(50);
    testEqual(v.size(), 50);
    for (int i = 10; i < 50; i++)
        v[i] = i;

    for (int i = 0; i < 50; i++)
        testEqual(v[i], i);

    v.resize(30);
    testEqual(v.size(), 30);
    v.resize(0);
    testEqual(v.size(), 0);
}

template <typename V>
static void testCopy()
{
    V a;
    V b(a);
    testTrue(b.empty());

    populate(a, 10);
    V c(a);
    checkContents(c, 10);
}

template <typename V>
static void checkIterators(V& v)
{
    auto i = v.begin();
    auto j = v.end();
    testEqual(*i, 0);

    testTrue(i == i);
    testFalse(i != i);
    testTrue(j == j);
    testFalse(j != j);
    testFalse(i == j);
    testTrue(i != j);
    testTrue(i < j);
    testTrue(i <= j);
    testFalse(i > j);
    testFalse(i >= j);

    testEqual(*++i, 1);
    testEqual(*--i, 0);
    testEqual(*(i + 2), 2);
    testEqual(*(j - 1), 9);
    testEqual(j - i, 10);
    i += 3;
    testEqual(*i, 3);
    i -= 1;
    testEqual(*i, 2);
}

template <typename V>
static void testIterators()
{
    V v;
    populate(v, 10);
    checkIterators(v);
    const V& cv = v;
    checkIterators(cv);
}

template <typename V>
static void testErase1()
{
    V v;
    populate(v, 10);
    auto i = v.erase(v.begin());
    testEqual(v.size(), 9);
    testEqual(v[0], 1);
    testTrue(i == v.begin());

    i = v.erase(v.end() - 1);
    testEqual(v.size(), 8);
    testEqual(v[7], 8);
    testTrue(i == v.end());

    testEqual(v[2], 3);
    testEqual(v[3], 4);
    testEqual(v[4], 5);
    v.erase(v.begin() + 3);
    testEqual(v.size(), 7);
    testEqual(v[2], 3);
    testEqual(v[3], 5);
    testEqual(v[4], 6);
}

template <typename V>
static void testErase2()
{
    V v;
    populate(v, 20);
    testEqual(v[0], 0);
    testEqual(v[19], 19);

    v.erase(v.begin(), v.begin());
    checkContents(v, 20);

    auto i = v.erase(v.begin(), v.begin() + 5);
    testEqual(v.size(), 15);
    testEqual(v[0], 5);
    testEqual(v[14], 19);
    testTrue(i == v.begin());

    i = v.erase(v.end() - 5, v.end());
    testEqual(v.size(), 10);
    testEqual(v[0], 5);
    testEqual(v[9], 14);
    testTrue(i == v.end());

    i = v.erase(v.begin() + 1, v.end() - 1);
    testEqual(v.size(), 2);
    testEqual(v[0], 5);
    testEqual(v[1], 14);
    testTrue(i == v.begin() + 1);

    i = v.erase(v.begin(), v.end());
    testTrue(v.empty());
    testTrue(i == v.end());
}

template <typename V>
static void testInsert1()
{
    V v;
    populate(v, 10);
    testEqual(v[2], 2);
    testEqual(v[3], 3);
    testEqual(v[4], 4);

    auto i = v.insert(v.begin() + 3, 0);
    testEqual(v[2], 2);
    testEqual(v[3], 0);
    testEqual(v[4], 3);
    testTrue(i == v.begin() + 3);
}

template <typename V>
static void testInsert2()
{
    V src;
    populate(src, 10);

    V v;
    v.insert(v.begin(), src.begin(), src.begin());
    testEqual(v.size(), 0);

    auto i = v.insert(v.begin(), src.begin(), src.end());
    checkContents(v, 10);
    testTrue(i == v.begin());

    v.erase(v.begin(), v.begin() + 5);
    i = v.insert(v.begin(), src.begin(), src.begin() + 5);
    checkContents(v, 10);
    testTrue(i == v.begin());

    populate(src, 20);
    i = v.insert(v.end(), src.begin() + 10, src.end());
    checkContents(v, 20);
    testTrue(i == v.begin() + 10);
}

template <typename V>
static void testEmplaceBack()
{
    V v;
    v.emplace_back(0);
    testEqual(v.size(), 1);
    testEqual(v[0], 0);
}

testcase(vector)
{
    testPush<std::vector<Element>>(30);
    testEqual(Element::Count, 0);
    testPush<Vector<Element>>(30);
    testEqual(Element::Count, 0);
    testPush<InlineVector<Element, 10>>(100);
    testEqual(Element::Count, 0);

    testFill<std::vector<Element>>(0, 0);
    testEqual(Element::Count, 0);
    testFill<std::vector<Element>>(10, 9);
    testEqual(Element::Count, 0);
    testFill<Vector<Element>>(0, 0);
    testEqual(Element::Count, 0);
    testFill<Vector<Element>>(10, 9);
    testEqual(Element::Count, 0);
    testFill<InlineVector<Element, 4>>(2, 9);
    testEqual(Element::Count, 0);
    testFill<InlineVector<Element, 4>>(10, 9);
    testEqual(Element::Count, 0);
    testFill<InlineVector<Element, 4>>(100, 9);
    testEqual(Element::Count, 0);

    testCopy<std::vector<Element>>();
    testEqual(Element::Count, 0);
    testCopy<Vector<Element>>();
    testEqual(Element::Count, 0);
    testCopy<InlineVector<Element, 4>>();
    testEqual(Element::Count, 0);

    testResize<std::vector<Element>>();
    testEqual(Element::Count, 0);
    testResize<Vector<Element>>();
    testEqual(Element::Count, 0);
    testResize<InlineVector<Element, 10>>();
    testEqual(Element::Count, 0);

    testIterators<std::vector<Element>>();
    testEqual(Element::Count, 0);
    testIterators<Vector<Element>>();
    testEqual(Element::Count, 0);
    testIterators<InlineVector<Element, 10>>();
    testEqual(Element::Count, 0);

    testErase1<std::vector<Element>>();
    testEqual(Element::Count, 0);
    testErase1<Vector<Element>>();
    testEqual(Element::Count, 0);
    testErase1<InlineVector<Element, 3>>();
    testEqual(Element::Count, 0);

    testErase2<std::vector<Element>>();
    testEqual(Element::Count, 0);
    testErase2<Vector<Element>>();
    testEqual(Element::Count, 0);
    testErase2<InlineVector<Element, 10>>();
    testEqual(Element::Count, 0);

    testInsert1<std::vector<Element>>();
    testEqual(Element::Count, 0);
    testInsert1<Vector<Element>>();
    testEqual(Element::Count, 0);
    testInsert1<InlineVector<Element, 4>>();
    testEqual(Element::Count, 0);

    testInsert2<std::vector<Element>>();
    testEqual(Element::Count, 0);
    testInsert2<Vector<Element>>();
    testEqual(Element::Count, 0);
    testInsert2<InlineVector<Element, 10>>();
    testEqual(Element::Count, 0);

    testEmplaceBack<std::vector<Element>>();
    testEqual(Element::Count, 0);
    testEmplaceBack<InlineVector<Element, 4>>();
    testEqual(Element::Count, 0);
}
