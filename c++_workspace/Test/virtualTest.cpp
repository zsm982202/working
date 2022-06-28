
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
	//�������ض��� �����ظ���ͬ������
	void func1() {
		cout << "fish func1" << endl;
	}
	//��������д�� ���Ǹ���ķ��� override
	void func2(int tmp) {
		cout << "fish func2-" << tmp << endl;
	}
	//�������ض��� �����ظ���ͬ������
	void func3(int tmp) {
		cout << "fish func3-" << tmp << endl;
	}
};

int main() {
	Fish fi;
	Animal an;

	fi.func1();
	//fish func1
	// �������ض��� ����ķ����Ѿ������� 
	// ��Ҫ��ʾ���������ز��ܿ�������
	fi.Animal::func1(1);
	//animal func1-1
	dynamic_cast<Animal*>(&fi)->func1(11); // ǿת֮�󼴿ɵ��õ����౻���صķ���
	//animal func1-11
	dynamic_cast<Animal*>(&fi)->func1("hello world"); // ǿת֮�󼴿ɵ��õ����౻���صķ���
	//animal func1-hello world
	fi.func2(2);	// ��������
	//fish func2-2
	dynamic_cast<Animal*>(&fi)->func2(22); // ����"���෽��"(��Ϊ���麯�����ᱻ���า��)
	//fish func2-22
	dynamic_cast<Animal*>(&fi)->func3(222); // ���ø���
	//animal func3-222
	fi.func3(2222);	// ��������
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

