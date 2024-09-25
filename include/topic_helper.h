#pragma once
#include <string>

/*定义topic全局变量，全量topic全局变量、get函数和获取topic哈希的函数
str是局部主题字符串，name是获取全部主题字符串（已添加前缀）的函数名

例如DEF_TOPIC("admin/res", admin_res)，相当于
sring_view _admin_res_str = "admin/res";
string     _admin_res_topic;
string_view admin_res() {return _admin_res_topic;}
constexpr uint32_t admin_res_hash() {return "admin/res"_hash;}
*/

#define DEF_TOPIC(str, name)	static string_view _##name##_str = str;						\
								static std::string _##name##_topic;							\
								string_view name() {return _##name##_topic;}				\
								// constexpr uint32_t name##_hash() {return str##_hash;}

//由于string需要使用堆，因此全量topic的获取只能在init函数中执行
#define INIT_TOPIC(type, name) _##name##_topic = topic_helper::get_full_topic_##type(_##name##_str)

namespace topic_helper {

//主题前缀长度
uint32_t nexblue_prefix_length();
uint32_t shadow_prefix_length();
//生成mqtt主题：'NB/dev/SN/b7b7259b090f4e4daa95090ee8d28ec9/req'
std::string get_full_topic_circuit(std::string_view circuit_id);
//生成mqtt主题：'model/env/SN/field'
std::string get_full_topic_nexblue(std::string_view field);
//生成mqtt主题：'$aws/things/NBFFA10080/shadow/name/NB1620A/field'
std::string get_full_topic_shadow(std::string_view field);
//生成$aws/rules/前缀的主题，目前仅用于生成creq，因为它需要aws规则引擎筛选
std::string get_full_topic_aws(std::string_view field, std::string_view sn, std::string_view env);

void init(std::string_view sn, std::string_view env_str);
}
