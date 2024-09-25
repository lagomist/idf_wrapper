#include "topic_helper.h"

namespace topic_helper {

//生成mqtt主题前缀：'model/env/SN/'
static std::string _nexblue_prefix;
static std::string _shadow_prefix;

//获取桩的当前模式（用SN的前两位表示）
static std::string_view get_model(std::string_view sn) {
	return sn.substr(0, 2);
}

//生成mqtt主题前缀：'model/env/SN/'
static std::string generate_nexblue_prefix(std::string_view sn, std::string_view env) {
	std::string topic = "";
	topic.append(get_model(sn));
	topic.append("/");
	topic.append(env);
	topic.append("/");
	topic.append(sn);
	topic.append("/");
	return topic;
}

//生成mqtt主题前缀：'$aws/things/NBFFA10080/shadow/name/NB1620A'
static std::string generate_shadow_prefix(std::string_view sn) {
    std::string topic;
    topic.append("$aws/things/");
    topic.append(sn);
	#if (defined(PROJECT_SMBNA))
	topic.append("/shadow/name/CS3ANA/");
	#elif (defined(PROJECT_NB1620A) || defined(PROJECT_NB1670A) || defined(PROJECT_NB2670A))
    topic.append("/shadow/name/NB1620A/");
	#else
    topic.append("/shadow/name/" PROJECT_NAME "/");
	#endif
	return topic;
}

uint32_t nexblue_prefix_length() {
	return _nexblue_prefix.size();
}

uint32_t shadow_prefix_length() {
	return _shadow_prefix.size();
}

std::string get_full_topic_circuit(std::string_view circuit_id) {
	std::string topic = "";
	topic.append(_nexblue_prefix);
	topic.append(circuit_id);
	topic.append("/req");
	return topic;
}

std::string get_full_topic_nexblue(std::string_view field) {
	std::string topic = "";
	topic.append(_nexblue_prefix);
	topic.append(field);
	return topic;
}

std::string get_full_topic_shadow(std::string_view field) {
    std::string topic = "";
    topic.append(_shadow_prefix);
    topic.append(field);
    return topic;
}

std::string get_full_topic_aws(std::string_view field, std::string_view sn, std::string_view env) {
	std::string topic = "$aws/rules/";
	topic.append(env);
	topic.append("_");
	topic.append(field);
	topic.append("/");
	topic.append(get_model(sn));
	topic.append("/");
	topic.append(sn);
	return topic;
}

void init(std::string_view sn, std::string_view env) {
	_nexblue_prefix = generate_nexblue_prefix(sn, env);
	_shadow_prefix = generate_shadow_prefix(sn);
}

	
} // namespace topic_helper
