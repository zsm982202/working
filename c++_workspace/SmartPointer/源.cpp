#include <iostream>
#include <memory>

using namespace std;

//int main() {
//    {
//        int a = 10;
//        std::shared_ptr<int> ptra = std::make_shared<int>(a);
//        std::shared_ptr<int> ptra2(ptra); //copy
//        std::cout << ptra.use_count() << std::endl;
//
//        int b = 20;
//        int* pb = &a;
//        //std::shared_ptr<int> ptrb = pb;  //error
//        std::shared_ptr<int> ptrb = std::make_shared<int>(b);
//        ptra2 = ptrb; //assign
//        pb = ptrb.get(); //获取原始指针
//
//        std::cout << ptra.use_count() << std::endl;
//        std::cout << ptrb.use_count() << std::endl;
//    }
//    return 0;
//}

//class B; //声明
//class A {
//public:
//	weak_ptr<B> pb_;
//	~A() {
//		cout << "A delete\n";
//	}
//};
//
//class B {
//public:
//	shared_ptr<A> pa_;
//	~B() {
//		cout << "B delete\n";
//	}
//};
//
//void fun() {
//	shared_ptr<B> pb(new B());
//	shared_ptr<A> pa(new A());
//	cout << pb.use_count() << endl; //1
//	cout << pa.use_count() << endl; //1
//	pb->pa_ = pa;
//	pa->pb_ = pb;
//	cout << pb.use_count() << endl; //1
//	cout << pa.use_count() << endl; //2
//	//B delete
//	//A delete
//}
//
//int main() {
//	fun();
//	return 0;
//}

//template <class T>
//class WeakPtr; //为了用weak_ptr的lock()，来生成share_ptr用，需要拷贝构造用
//
//template <class T>
//class SharePtr {
//public:
//    SharePtr(T* p = 0): _ptr(p) {
//        cnt = new Counter();
//        if(p)
//            cnt->s = 1;
//        cout << "in construct " << cnt->s << endl;
//    }
//    ~SharePtr() {
//        release();
//    }
//
//    SharePtr(SharePtr<T> const& s) {
//        cout << "in copy con" << endl;
//        _ptr = s._ptr;
//        (s.cnt)->s++;
//        cout << "copy construct" << (s.cnt)->s << endl;
//        cnt = s.cnt;
//    }
//    SharePtr(WeakPtr<T> const& w) //为了用weak_ptr的lock()，来生成share_ptr用，需要拷贝构造用
//    {
//        cout << "in w copy con " << endl;
//        _ptr = w._ptr;
//        (w.cnt)->s++;
//        cout << "copy w  construct" << (w.cnt)->s << endl;
//        cnt = w.cnt;
//    }
//    SharePtr<T>& operator=(SharePtr<T>& s) {
//        if(this != &s) {
//            release();
//            (s.cnt)->s++;
//            cout << "assign construct " << (s.cnt)->s << endl;
//            cnt = s.cnt;
//            _ptr = s._ptr;
//        }
//        return *this;
//    }
//    T& operator*() {
//        return *_ptr;
//    }
//    T* operator->() {
//        return _ptr;
//    }
//    friend class WeakPtr<T>; //方便weak_ptr与share_ptr设置引用计数和赋值
//
//protected:
//    void release() {
//        cnt->s--;
//        cout << "release " << cnt->s << endl;
//        if(cnt->s < 1) {
//            delete _ptr;
//            if(cnt->w < 1) {
//                delete cnt;
//                cnt = NULL;
//            }
//        }
//    }
//
//private:
//    T* _ptr;
//    Counter* cnt;
//};
//
//template <class T>
//class WeakPtr {
//public: //给出默认构造和拷贝构造，其中拷贝构造不能有从原始指针进行构造
//    WeakPtr() {
//        _ptr = 0;
//        cnt = 0;
//    }
//    WeakPtr(SharePtr<T>& s): _ptr(s._ptr), cnt(s.cnt) {
//        cout << "w con s" << endl;
//        cnt->w++;
//    }
//    WeakPtr(WeakPtr<T>& w): _ptr(w._ptr), cnt(w.cnt) {
//        cnt->w++;
//    }
//    ~WeakPtr() {
//        release();
//    }
//    WeakPtr<T>& operator=(WeakPtr<T>& w) {
//        if(this != &w) {
//            release();
//            cnt = w.cnt;
//            cnt->w++;
//            _ptr = w._ptr;
//        }
//        return *this;
//    }
//    WeakPtr<T>& operator=(SharePtr<T>& s) {
//        cout << "w = s" << endl;
//        release();
//        cnt = s.cnt;
//        cnt->w++;
//        _ptr = s._ptr;
//        return *this;
//    }
//    SharePtr<T> lock() {
//        return SharePtr<T>(*this);
//    }
//    bool expired() {
//        if(cnt) {
//            if(cnt->s > 0) {
//                cout << "empty" << cnt->s << endl;
//                return false;
//            }
//        }
//        return true;
//    }
//    friend class SharePtr<T>; //方便weak_ptr与share_ptr设置引用计数和赋值
//
//protected:
//    void release() {
//        if(cnt) {
//            cnt->w--;
//            cout << "weakptr release" << cnt->w << endl;
//            if(cnt->w < 1 && cnt->s < 1) {
//                //delete cnt;
//                cnt = NULL;
//            }
//        }
//    }
//
//private:
//    T* _ptr;
//    Counter* cnt;
//};
#include <assert.h>
#include <vector>
void fun(const int& a) {
	//a = 10;
}
class A {
public:
	static int a;
	//A(int b):a(b) {}
};
int A::a = 10;
int main() {
	//int a = 1;
	//int b = 2;
	//int const* p = &a;
	////*p = 3;
	//p = &b;
	//cout << *p << endl;
	//fun(a);
	//A a1;
	//cout << a1.a << endl;
	//assert(0);

	/*int a = 10;
	int&& b = move(a);
	b = 11;
	cout << a << endl;*/

	int e = 1, f = 2;
	int* c = &e;
	int* d = &f;
	vector<int*> a { c,d };
	vector<int*> b = a;
	*a[1] = 10;
	for(int i = 0; i < b.size(); i++)
		cout << *b[i] << " ";
	cout << endl;
	return 0;
}