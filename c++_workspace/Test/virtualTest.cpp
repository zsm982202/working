
#include<iostream>
#include "t.h"

using namespace std;

class Animal {
public:
	/*virtual Animal() {
	
	}*/

	void func1(int tmp) {
		cout << "animal func1-" << tmp << endl;
	}

	void func1(const char* s) {
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
	//函数的重定义 会隐藏父类同名方法
	void func1() {
		cout << "fish func1" << endl;
	}
	//函数的重写， 覆盖父类的方法 override
	void func2(int tmp) {
		cout << "fish func2-" << tmp << endl;
	}
	//函数的重定义 会隐藏父类同名方法
	void func3(int tmp) {
		cout << "fish func3-" << tmp << endl;
	}
};

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
	return 0;
}

