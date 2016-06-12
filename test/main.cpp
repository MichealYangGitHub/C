#include <vector>
#include <iostream>

using namespace std;

template<typename T>
void testT(T out)
{
    cout<<out.size();
}

int main(int argv, char **args)
{
    cout<<"====start======"<<endl;
    vector<char> out;
    cout<<out.size()<<endl;
    cout<<(out.begin()==out.end())<<endl;
    out.resize(5);
    cout<<out.size()<<endl;
    cout<<(out.begin()==out.end())<<endl;
    cout<<*out.begin()<<endl;
}
