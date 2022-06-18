
#include<iostream>
#include "t.h"

using namespace std;

class Animal {
public:
	void func1(int tmp) {
		cout << "animal func1-" << tmp << endl;
	}

	void func1(const char* s)//函数的重载
	{
		cout << "animal func1-" << s << endl;
	}

	virtual void func2(int tmp) {
		cout << "animal virtual func2-" << tmp << endl;
	}

	void func3(int tmp) {
		cout << "animal func3-" << tmp << endl;
	}
};

class Fish :public Animal {
public:
	void func1()//函数的重定义 会隐藏父类同名方法
	{
		cout << "fish func1" << endl;
	}

	void func2(int tmp) //函数的重写， 覆盖父类的方法 override
	{
		cout << "fish func2-" << tmp << endl;
	}

	void func3(int tmp) { //函数的重定义 会隐藏父类同名方法
		cout << "fish func3-" << tmp << endl;
	}
};

//extern int a[];
//extern int b;

int main() {
	Fish fi;
	Animal an;

	fi.func1();
	//fish func1
	// 由于是重定义 父类的方法已经被隐藏 
	// 需要显示声明，重载不能跨作用域
	fi.Animal::func1(1);
	//animal func1-1
	dynamic_cast<Animal*>(&fi)->func1(11); // 强转之后即可调用到父类被隐藏的方法
	//animal func1-11
	dynamic_cast<Animal*>(&fi)->func1("hello world"); // 强转之后即可调用到父类被隐藏的方法
	//animal func1-hello world
	fi.func2(2);	// 调用子类
	//fish func2-2
	dynamic_cast<Animal*>(&fi)->func2(22); // 调用"子类方法"(因为是虚函数，会被子类覆盖)
	//fish func2-22
	dynamic_cast<Animal*>(&fi)->func3(222); // 调用父类
	//animal func3-222
	fi.func3(2222);	// 调用子类
	//fish func3-2222

	cout << endl << " ************ " << endl;
	an.func1(1);
	//animal func1-1
	an.func1("I'm an animal");
	//animal func1-I'm an animal
	an.func2(1);
	//animal virtual func2-1
	// 
	
	////system("pause");

	//cout << endl;
	//const int i = 1;
	//const int a = i;
	//int* p = const_cast<int*>(&a);
	//(*p)++;
	//cout << *p << endl;
	//cout << a << endl;
	//cout << endl;

	//int num = 0x00636261;//用16进制表示32位int，0x61是字符'a'的ASCII码
	//int* pnum = &num;
	//// char* pstr = reinterpret_cast<char*>(pnum);
	//char* pstr = (char*)(pnum);
	//cout << "pnum指针的值: " << pnum << endl;
	//cout << "pstr指针的值: " << static_cast<void*>(pstr) << endl;//直接输出pstr会输出其指向的字符串，这里的类型转换是为了保证输出pstr的值
	//cout << "pnum指向的内容: " << hex << *pnum << endl;
	//cout << "pstr指向的内容: " << pstr << endl;
	/*extern int a[];
	cout << a[0] << endl;*/
	//cout << b << endl;
	return 0;
}