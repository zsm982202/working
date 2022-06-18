
#include<iostream>
#include "t.h"

using namespace std;

class Animal {
public:
	void func1(int tmp) {
		cout << "animal func1-" << tmp << endl;
	}

	void func1(const char* s)//����������
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
	void func1()//�������ض��� �����ظ���ͬ������
	{
		cout << "fish func1" << endl;
	}

	void func2(int tmp) //��������д�� ���Ǹ���ķ��� override
	{
		cout << "fish func2-" << tmp << endl;
	}

	void func3(int tmp) { //�������ض��� �����ظ���ͬ������
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

	//int num = 0x00636261;//��16���Ʊ�ʾ32λint��0x61���ַ�'a'��ASCII��
	//int* pnum = &num;
	//// char* pstr = reinterpret_cast<char*>(pnum);
	//char* pstr = (char*)(pnum);
	//cout << "pnumָ���ֵ: " << pnum << endl;
	//cout << "pstrָ���ֵ: " << static_cast<void*>(pstr) << endl;//ֱ�����pstr�������ָ����ַ��������������ת����Ϊ�˱�֤���pstr��ֵ
	//cout << "pnumָ�������: " << hex << *pnum << endl;
	//cout << "pstrָ�������: " << pstr << endl;
	/*extern int a[];
	cout << a[0] << endl;*/
	//cout << b << endl;
	return 0;
}