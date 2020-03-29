#include <string>
#include <vector>
/**
	class to get initial list for the sockets to be listened for
	@note: ip 0.0.0.0 means that for that port all ip adresses will be listened to
*/
class ConfigList
{
public:
	//Ctor
	//Add or edit ip's and port you want to be forwarded
	ConfigList()
	{
		socketList.clear();
		socketList.push_back(std::make_pair<std::string, unsigned short>("0.0.0.0", 80));
		socketList.push_back(std::make_pair<std::string, unsigned short>("0.0.0.0", 20));
		socketList.push_back(std::make_pair<std::string, unsigned short>("0.0.0.0", 30));
		socketList.push_back(std::make_pair<std::string, unsigned short>("0.0.0.0", 50));
		socketList.push_back(std::make_pair<std::string, unsigned short>("0.0.0.0", 100));
		socketList.push_back(std::make_pair<std::string, unsigned short>("0.0.0.0", 54000));
	}
	//Dtor
	~ConfigList()
	{

	}


	/**
	@return ref to vector containing pair's of ip string and  port
	*/
	std::vector<std::pair<std::string, unsigned short>>& getList()
	{
		return socketList;
	}



private:

	/*static*/ std::vector<std::pair<std::string, unsigned short>> socketList;
};

