#include "ChatInputEdit.h"

#include <QApplication>
#include <QFile>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QScreen>
#include <QSet>
#include <QStringConverter>
#include <QTextCursor>
#include <QTextLayout>
#include <QTextStream>
#include <QVector>

#include <algorithm>

#include "Theme.h"

namespace {

using InputMode = ChatInputEdit::InputMode;

InputMode gInputMode = InputMode::Chinese;
QSet<ChatInputEdit *> gInputEdits;

struct PinyinEntry {
    const char *key;
    const char *candidates;
};

constexpr int kMaxPinyinCandidatesPerKey = 5;
constexpr const char kPinyinDictResourcePath[] = ":/mi/e2ee/ui/ime/pinyin.dat";
constexpr const char kEnglishDictResourcePath[] = ":/mi/e2ee/ui/ime/english.dat";

static const PinyinEntry kPinyinDict[] = {
    {"a", u8"\u554a|\u963f|\u5475"},
    {"ai", u8"\u7231|\u827e|\u54c0|\u6328"},
    {"an", u8"\u5b89|\u6309|\u6848|\u6697|\u5cb8|\u4ffa"},
    {"ang", u8"\u6602|\u76ce"},
    {"ao", u8"\u5965|\u6fb3|\u71ac|\u50b2|\u51f9"},
    {"ba", u8"\u628a|\u516b|\u5427|\u7238|\u5df4|\u62d4"},
    {"bai", u8"\u767d|\u767e|\u6446|\u8d25|\u62dc"},
    {"ban", u8"\u534a|\u73ed|\u529e|\u677f|\u822c|\u4f34"},
    {"bang", u8"\u5e2e|\u90a6|\u699c|\u68d2|\u508d"},
    {"bao", u8"\u5305|\u62a5|\u5b9d|\u4fdd|\u66b4|\u62b1"},
    {"bei", u8"\u5317|\u88ab|\u5907|\u676f|\u500d|\u60b2"},
    {"ben", u8"\u672c|\u5954|\u7b28"},
    {"beng", u8"\u5d29|\u6cf5|\u8e66"},
    {"bi", u8"\u6bd4|\u5fc5|\u7b14|\u5f7c|\u903c|\u9f3b|\u58c1"},
    {"bian", u8"\u53d8|\u8fb9|\u7f16|\u4fbf|\u904d|\u8fa8|\u6241"},
    {"biao", u8"\u8868|\u6807|\u5f6a|\u98d9"},
    {"bie", u8"\u522b|\u618b"},
    {"bin", u8"\u5bbe|\u5f6c|\u658c"},
    {"bing", u8"\u5e76|\u75c5|\u5175|\u51b0"},
    {"bo", u8"\u6ce2|\u535a|\u64ad|\u4f2f|\u62e8"},
    {"bu", u8"\u4e0d|\u90e8|\u5e03|\u6b65|\u8865|\u6355"},
    {"ca", u8"\u64e6|\u5693"},
    {"cai", u8"\u624d|\u8d22|\u91c7|\u83dc"},
    {"can", u8"\u53c2|\u6b8b|\u9910|\u60ed"},
    {"cang", u8"\u4ed3|\u85cf|\u82cd"},
    {"cao", u8"\u8349|\u64cd|\u66f9"},
    {"ce", u8"\u4fa7|\u6d4b|\u518c"},
    {"cen", u8"\u5c91"},
    {"ceng", u8"\u5c42|\u66fe"},
    {"cha", u8"\u67e5|\u5dee|\u63d2|\u8336|\u53c9"},
    {"chai", u8"\u67f4|\u62c6|\u5dee"},
    {"chan", u8"\u4ea7|\u7f20|\u7985|\u9610"},
    {"chang", u8"\u957f|\u5e38|\u573a|\u5531|\u5382"},
    {"chao", u8"\u8d85|\u671d|\u6f6e|\u7092"},
    {"che", u8"\u8f66|\u64a4|\u5f7b|\u626f"},
    {"chen", u8"\u9648|\u6668|\u6c89|\u5c18|\u81e3"},
    {"cheng", u8"\u6210|\u57ce|\u7a0b|\u79f0|\u627f|\u4e58"},
    {"chi", u8"\u5403|\u8fdf|\u5c3a|\u6301|\u6c60"},
    {"chong", u8"\u51b2|\u866b|\u5145|\u91cd"},
    {"chou", u8"\u62bd|\u6101|\u4ec7|\u4e11|\u7b79"},
    {"chu", u8"\u51fa|\u5904|\u521d|\u9664|\u89e6|\u695a"},
    {"chuan", u8"\u4f20|\u7a7f|\u8239|\u5ddd"},
    {"chuang", u8"\u7a97|\u5e8a|\u521b|\u95ef"},
    {"chui", u8"\u5439|\u5782|\u9524"},
    {"chun", u8"\u6625|\u7eaf|\u5507|\u8822"},
    {"chuo", u8"\u6233|\u7ef0"},
    {"ci", u8"\u6b21|\u6b64|\u8bcd|\u8f9e|\u523a"},
    {"cong", u8"\u4ece|\u4e1b|\u806a|\u5306"},
    {"cou", u8"\u51d1"},
    {"cu", u8"\u7c97|\u4fc3|\u918b|\u7c07"},
    {"cuan", u8"\u7a9c|\u6512"},
    {"cui", u8"\u50ac|\u8106|\u7fe0|\u6467"},
    {"cun", u8"\u5b58|\u6751|\u5bf8"},
    {"cuo", u8"\u9519|\u63aa|\u632b|\u6413"},
    {"da", u8"\u5927|\u6253|\u8fbe|\u7b54|\u642d"},
    {"dai", u8"\u5e26|\u4ee3|\u6234|\u5f85|\u888b"},
    {"dan", u8"\u4f46|\u5355|\u86cb|\u62c5|\u80c6"},
    {"dang", u8"\u5f53|\u515a|\u6863|\u6321|\u8361"},
    {"dao", u8"\u5230|\u9053|\u5012|\u5200|\u5bfc|\u5c9b"},
    {"de", u8"\u7684|\u5f97|\u5730|\u5fb7"},
    {"dei", u8"\u5f97"},
    {"deng", u8"\u7b49|\u706f|\u767b|\u9093"},
    {"di", u8"\u5730|\u7b2c|\u5e95|\u4f4e|\u5f1f|\u654c"},
    {"dian", u8"\u70b9|\u7535|\u5e97|\u5178|\u57ab"},
    {"diao", u8"\u8c03|\u6389|\u96d5|\u9493"},
    {"die", u8"\u7239|\u8dcc|\u53e0|\u8776"},
    {"ding", u8"\u5b9a|\u9876|\u4e01|\u8ba2|\u9489"},
    {"diu", u8"\u4e22"},
    {"dong", u8"\u4e1c|\u52a8|\u61c2|\u51ac|\u6d1e"},
    {"dou", u8"\u90fd|\u6597|\u8c46|\u9017"},
    {"du", u8"\u8bfb|\u5ea6|\u72ec|\u6bd2|\u6e21"},
    {"duan", u8"\u6bb5|\u77ed|\u7aef|\u65ad"},
    {"dui", u8"\u5bf9|\u961f|\u5806"},
    {"dun", u8"\u987f|\u76fe|\u6566|\u8e72"},
    {"duo", u8"\u591a|\u593a|\u6735|\u8eb2|\u8235"},
    {"e", u8"\u989d|\u4fc4|\u6076|\u997f"},
    {"en", u8"\u6069|\u6441"},
    {"er", u8"\u4e8c|\u800c|\u513f|\u8033"},
    {"fa", u8"\u53d1|\u6cd5|\u4e4f|\u7f5a"},
    {"fan", u8"\u53cd|\u996d|\u8303|\u7ffb|\u70e6"},
    {"fang", u8"\u65b9|\u623f|\u653e|\u9632|\u8bbf"},
    {"fei", u8"\u975e|\u98de|\u8d39|\u5e9f|\u80a5"},
    {"fen", u8"\u5206|\u4efd|\u7eb7|\u7c89|\u594b"},
    {"feng", u8"\u98ce|\u5c01|\u5cf0|\u4e30|\u75af"},
    {"fo", u8"\u4f5b"},
    {"fou", u8"\u5426"},
    {"fu", u8"\u670d|\u590d|\u4ed8|\u798f|\u526f|\u8d1f|\u592b|\u9644|\u7b26"},
    {"ga", u8"\u560e|\u5c2c"},
    {"gai", u8"\u8be5|\u6539|\u76d6|\u6982"},
    {"gan", u8"\u5e72|\u611f|\u8d76|\u6562|\u7518"},
    {"gang", u8"\u521a|\u94a2|\u6e2f|\u5c97|\u7eb2"},
    {"gao", u8"\u9ad8|\u544a|\u641e|\u7a3f|\u818f"},
    {"ge", u8"\u4e2a|\u5404|\u6b4c|\u683c|\u54e5|\u5272"},
    {"gei", u8"\u7ed9"},
    {"gen", u8"\u8ddf|\u6839"},
    {"geng", u8"\u66f4|\u8015|\u5e9a|\u803f"},
    {"gong", u8"\u5de5|\u516c|\u5171|\u529f|\u653b"},
    {"gou", u8"\u591f|\u6784|\u6c9f|\u72d7|\u8d2d"},
    {"gu", u8"\u53e4|\u6545|\u987e|\u9f13|\u9aa8|\u8c37"},
    {"gua", u8"\u6302|\u522e|\u74dc|\u5be1"},
    {"guai", u8"\u602a|\u62d0|\u4e56"},
    {"guan", u8"\u5173|\u7ba1|\u5b98|\u89c2|\u9986"},
    {"guang", u8"\u5149|\u5e7f|\u901b"},
    {"gui", u8"\u5f52|\u8d35|\u9b3c|\u67dc|\u89c4"},
    {"gun", u8"\u6eda|\u68cd"},
    {"guo", u8"\u56fd|\u8fc7|\u679c|\u9505|\u90ed"},
    {"ha", u8"\u54c8"},
    {"hai", u8"\u8fd8|\u6d77|\u5bb3|\u5b69"},
    {"han", u8"\u6c49|\u542b|\u5bd2|\u558a|\u6c57"},
    {"hang", u8"\u884c|\u822a|\u676d|\u5df7"},
    {"hao", u8"\u597d|\u53f7|\u6d69|\u8c6a|\u8017"},
    {"he", u8"\u548c|\u5408|\u4f55|\u559d|\u6cb3|\u6838"},
    {"hei", u8"\u9ed1|\u563f"},
    {"hen", u8"\u5f88|\u72e0|\u6068"},
    {"heng", u8"\u6a2a|\u6052|\u8861"},
    {"hong", u8"\u7ea2|\u6d2a|\u5b8f|\u8f70"},
    {"hou", u8"\u540e|\u5019|\u539a|\u7334"},
    {"hu", u8"\u4e92|\u62a4|\u80e1|\u6e56|\u547c|\u6237|\u864e"},
    {"hua", u8"\u8bdd|\u82b1|\u5316|\u753b|\u534e"},
    {"huai", u8"\u574f|\u6000|\u6dee"},
    {"huan", u8"\u8fd8|\u6362|\u6b22|\u73af|\u7f13"},
    {"huang", u8"\u9ec4|\u614c|\u7687|\u6643|\u8352"},
    {"hui", u8"\u4f1a|\u56de|\u7070|\u6325|\u6c47|\u60e0"},
    {"hun", u8"\u6df7|\u5a5a|\u9b42"},
    {"huo", u8"\u6216|\u6d3b|\u706b|\u8d27|\u83b7"},
    {"ji", u8"\u673a|\u7ea7|\u8bb0|\u53ca|\u51e0|\u6025|\u65e2|\u8ba1"},
    {"jia", u8"\u5bb6|\u52a0|\u67b6|\u4ef7|\u5047|\u4f73"},
    {"jian", u8"\u89c1|\u4ef6|\u5efa|\u7b80|\u51cf|\u68c0|\u575a"},
    {"jiang", u8"\u5c06|\u8bb2|\u6c5f|\u5956|\u964d|\u7586"},
    {"jiao", u8"\u53eb|\u4ea4|\u6559|\u89d2|\u8f83|\u811a"},
    {"jie", u8"\u63a5|\u8282|\u89e3|\u7ed3|\u754c|\u501f|\u59d0"},
    {"jin", u8"\u8fdb|\u4eca|\u91d1|\u8fd1|\u5c3d"},
    {"jing", u8"\u7ecf|\u4eac|\u7cbe|\u666f|\u51c0|\u9759|\u7adf"},
    {"jiong", u8"\u7a98"},
    {"jiu", u8"\u5c31|\u4e5d|\u4e45|\u65e7|\u9152|\u6551"},
    {"ju", u8"\u5c40|\u5177|\u4e3e|\u636e|\u805a|\u53e5|\u8ddd"},
    {"juan", u8"\u5377|\u6350|\u5708|\u5026|\u7737"},
    {"jue", u8"\u51b3|\u89c9|\u7edd|\u6398|\u7235"},
    {"jun", u8"\u519b|\u5747|\u541b|\u4fca|\u83cc"},
    {"ka", u8"\u5361|\u5496"},
    {"kai", u8"\u5f00|\u51ef|\u6168|\u6977"},
    {"kan", u8"\u770b|\u780d|\u520a|\u52d8|\u582a"},
    {"kang", u8"\u5eb7|\u6297|\u625b|\u6177"},
    {"kao", u8"\u9760|\u8003|\u70e4|\u94d0"},
    {"ke", u8"\u53ef|\u79d1|\u5ba2|\u523b|\u514b|\u8bfe"},
    {"ken", u8"\u80af|\u57a6|\u6073"},
    {"keng", u8"\u5751"},
    {"kong", u8"\u7a7a|\u5b54|\u63a7|\u6050"},
    {"kou", u8"\u53e3|\u6263|\u5bc7|\u53e9"},
    {"ku", u8"\u82e6|\u5e93|\u88e4|\u54ed"},
    {"kua", u8"\u8de8|\u5938|\u57ae"},
    {"kuai", u8"\u5feb|\u5757|\u7b77"},
    {"kuan", u8"\u5bbd|\u6b3e"},
    {"kuang", u8"\u51b5|\u72c2|\u6846|\u77ff|\u5321"},
    {"kui", u8"\u4e8f|\u594e|\u8475|\u9b41|\u9988"},
    {"kun", u8"\u56f0|\u6606|\u5764|\u6346"},
    {"kuo", u8"\u6269|\u9614|\u62ec"},
    {"la", u8"\u5566|\u62c9|\u8fa3|\u8721"},
    {"lai", u8"\u6765|\u8d56|\u83b1"},
    {"lan", u8"\u84dd|\u5170|\u70c2|\u680f|\u89c8"},
    {"lang", u8"\u6d6a|\u90ce|\u72fc|\u5eca"},
    {"lao", u8"\u8001|\u52b3|\u7262|\u635e"},
    {"le", u8"\u4e86|\u4e50|\u52d2"},
    {"lei", u8"\u7c7b|\u7d2f|\u96f7|\u6cea"},
    {"leng", u8"\u51b7|\u6123|\u68f1"},
    {"li", u8"\u91cc|\u7406|\u529b|\u5229|\u7acb|\u674e|\u4f8b|\u79bb|\u5386"},
    {"lian", u8"\u8fde|\u8054|\u8138|\u7ec3|\u94fe"},
    {"liang", u8"\u4e24|\u91cf|\u4eae|\u6881|\u826f"},
    {"liao", u8"\u4e86|\u6599|\u804a|\u7597|\u5ed6"},
    {"lie", u8"\u5217|\u70c8|\u88c2|\u730e"},
    {"lin", u8"\u6797|\u4e34|\u90bb|\u7433|\u78f7"},
    {"ling", u8"\u9886|\u4ee4|\u7075|\u96f6|\u9f84"},
    {"liu", u8"\u516d|\u6d41|\u7559|\u5218|\u67f3|\u6e9c"},
    {"long", u8"\u9f99|\u9686|\u7b3c|\u804b|\u5784"},
    {"lou", u8"\u697c|\u6f0f|\u9732|\u6402|\u5a04"},
    {"lu", u8"\u8def|\u5f55|\u5362|\u9732|\u9c81|\u9646|\u7089|\u9e7f"},
    {"luan", u8"\u4e71|\u5375"},
    {"lue", u8"\u7565|\u63a0"},
    {"lun", u8"\u8bba|\u8f6e|\u4f26"},
    {"luo", u8"\u843d|\u7f57|\u6d1b|\u7edc|\u903b"},
    {"lv", u8"\u7eff|\u7387|\u65c5"},
    {"ma", u8"\u5417|\u9a6c|\u9ebb|\u5988|\u7801"},
    {"mai", u8"\u4e70|\u5356|\u9ea6|\u8fc8"},
    {"man", u8"\u6ee1|\u6162|\u66fc|\u86ee|\u7792"},
    {"mang", u8"\u5fd9|\u832b|\u76f2|\u8292"},
    {"mao", u8"\u6bdb|\u732b|\u77db|\u5192|\u8d38|\u5e3d"},
    {"mei", u8"\u6ca1|\u6bcf|\u7f8e|\u6885|\u59b9|\u7164"},
    {"men", u8"\u4eec|\u95e8|\u95f7"},
    {"meng", u8"\u68a6|\u8499|\u731b|\u76df|\u5b5f"},
    {"mi", u8"\u7c73|\u5bc6|\u8ff7|\u5f25|\u79d8"},
    {"mian", u8"\u9762|\u514d|\u68c9|\u7720|\u7ef5"},
    {"miao", u8"\u79d2|\u82d7|\u63cf|\u5999"},
    {"mie", u8"\u706d|\u8511"},
    {"min", u8"\u6c11|\u654f|\u95fd|\u76bf"},
    {"ming", u8"\u660e|\u540d|\u547d|\u9e23|\u94ed"},
    {"mo", u8"\u6478|\u83ab|\u6a21|\u672b|\u78e8|\u58a8"},
    {"mou", u8"\u67d0|\u8c0b|\u725f"},
    {"mu", u8"\u76ee|\u6bcd|\u6728|\u5e55|\u7a46|\u7267"},
    {"na", u8"\u90a3|\u62ff|\u54ea|\u7eb3|\u5a1c"},
    {"nai", u8"\u4e43|\u5976|\u8010|\u5948"},
    {"nan", u8"\u96be|\u5357|\u7537|\u6960"},
    {"nang", u8"\u56ca|\u56d4"},
    {"nao", u8"\u8111|\u95f9|\u607c|\u6320"},
    {"ne", u8"\u5462|\u54ea"},
    {"nei", u8"\u5185|\u9981"},
    {"nen", u8"\u5ae9"},
    {"neng", u8"\u80fd"},
    {"ni", u8"\u4f60|\u6ce5|\u59ae|\u62df|\u9006"},
    {"nian", u8"\u5e74|\u5ff5|\u7c98|\u62c8"},
    {"niang", u8"\u5a18"},
    {"niao", u8"\u9e1f|\u5c3f"},
    {"nie", u8"\u634f|\u6d85|\u8042"},
    {"nin", u8"\u60a8"},
    {"ning", u8"\u5b81|\u51dd|\u62e7"},
    {"niu", u8"\u725b|\u626d|\u7ebd|\u94ae"},
    {"nong", u8"\u519c|\u5f04|\u6d53"},
    {"nu", u8"\u6012|\u52aa|\u5974"},
    {"nuan", u8"\u6696"},
    {"nuo", u8"\u8bfa|\u632a|\u61e6"},
    {"nv", u8"\u5973|\u9495"},
    {"nve", u8"\u8650|\u759f"},
    {"o", u8"\u54e6|\u5662"},
    {"ou", u8"\u6b27|\u5076|\u54e6|\u5455"},
    {"pa", u8"\u6015|\u722c|\u8db4|\u5e15"},
    {"pai", u8"\u6d3e|\u6392|\u724c|\u62cd"},
    {"pan", u8"\u76d8|\u5224|\u6f58|\u76fc|\u6500"},
    {"pang", u8"\u65c1|\u80d6|\u5e9e|\u8180"},
    {"pao", u8"\u8dd1|\u70ae|\u6ce1|\u888d"},
    {"pei", u8"\u966a|\u914d|\u4f69|\u57f9"},
    {"pen", u8"\u55b7|\u76c6"},
    {"peng", u8"\u670b|\u78b0|\u68da|\u9e4f"},
    {"pi", u8"\u76ae|\u6279|\u5426|\u75b2|\u5339|\u62ab|\u5288"},
    {"pian", u8"\u7247|\u7bc7|\u504f|\u9a97|\u4fbf"},
    {"piao", u8"\u7968|\u6f02|\u98d8|\u74e2"},
    {"pie", u8"\u6487|\u77a5"},
    {"pin", u8"\u62fc|\u9891|\u54c1|\u8d2b"},
    {"ping", u8"\u5e73|\u8bc4|\u74f6|\u51ed|\u840d"},
    {"po", u8"\u7834|\u5761|\u8feb|\u5a46|\u9887"},
    {"pu", u8"\u666e|\u94fa|\u6251|\u6d66|\u8c31"},
    {"qi", u8"\u8d77|\u5176|\u671f|\u6c14|\u4e03|\u9f50|\u5668|\u4f01|\u9a91"},
    {"qia", u8"\u5361|\u6070|\u6390"},
    {"qian", u8"\u524d|\u5343|\u94b1|\u7b7e|\u6b20|\u6d45|\u8fc1|\u8c26|\u7275"},
    {"qiang", u8"\u5f3a|\u62a2|\u5899|\u67aa|\u8154"},
    {"qiao", u8"\u6865|\u5de7|\u6084|\u6572|\u4e54"},
    {"qie", u8"\u5207|\u4e14|\u7a83|\u59be"},
    {"qin", u8"\u4eb2|\u7434|\u52e4|\u79e6|\u4fb5|\u5bdd"},
    {"qing", u8"\u8bf7|\u60c5|\u6e05|\u8f7b|\u5e86|\u6674"},
    {"qiong", u8"\u7a77"},
    {"qiu", u8"\u6c42|\u7403|\u79cb|\u4e18|\u56da"},
    {"qu", u8"\u53bb|\u533a|\u53d6|\u66f2|\u8da3|\u8d8b"},
    {"quan", u8"\u5168|\u6743|\u5708|\u529d|\u6cc9|\u62f3"},
    {"que", u8"\u5374|\u786e|\u7f3a|\u96c0"},
    {"qun", u8"\u7fa4|\u88d9"},
    {"ran", u8"\u7136|\u71c3|\u67d3"},
    {"rang", u8"\u8ba9|\u56b7|\u58e4"},
    {"rao", u8"\u7ed5|\u6270|\u9976"},
    {"re", u8"\u70ed|\u60f9"},
    {"ren", u8"\u4eba|\u4efb|\u8ba4|\u4ec1|\u5fcd"},
    {"reng", u8"\u4ecd|\u6254"},
    {"ri", u8"\u65e5"},
    {"rong", u8"\u5bb9|\u8363|\u878d|\u6eb6"},
    {"rou", u8"\u8089|\u67d4|\u63c9"},
    {"ru", u8"\u5165|\u5982|\u4e73|\u5112|\u8fb1"},
    {"ruan", u8"\u8f6f|\u962e"},
    {"rui", u8"\u745e|\u9510|\u777f"},
    {"run", u8"\u6da6|\u95f0"},
    {"ruo", u8"\u82e5|\u5f31"},
    {"sa", u8"\u6492|\u8428|\u6d12"},
    {"sai", u8"\u8d5b|\u585e|\u816e"},
    {"san", u8"\u4e09|\u6563|\u4f1e"},
    {"sang", u8"\u6851|\u4e27"},
    {"sao", u8"\u626b|\u9a9a|\u5ac2"},
    {"se", u8"\u8272|\u6da9|\u745f"},
    {"sen", u8"\u68ee"},
    {"seng", u8"\u50e7"},
    {"sha", u8"\u5565|\u6740|\u6c99|\u838e"},
    {"shai", u8"\u6652|\u7b5b"},
    {"shan", u8"\u5c71|\u95ea|\u5584|\u5220|\u6247|\u6749"},
    {"shang", u8"\u4e0a|\u5546|\u4f24|\u5c1a|\u8d4f"},
    {"shao", u8"\u5c11|\u70e7|\u7a0d|\u52fa|\u7ecd"},
    {"she", u8"\u793e|\u8bbe|\u820d|\u5c04|\u86c7|\u6444"},
    {"shen", u8"\u8eab|\u6df1|\u795e|\u4ec0|\u5ba1|\u4f38"},
    {"sheng", u8"\u751f|\u58f0|\u7701|\u80dc|\u5723"},
    {"shi", u8"\u662f|\u65f6|\u4e8b|\u5e02|\u8bd5|\u8bc6|\u4f7f|\u5341"},
    {"shou", u8"\u624b|\u6536|\u9996|\u53d7|\u6388"},
    {"shu", u8"\u4e66|\u6570|\u6811|\u719f|\u8f93|\u5c5e|\u675f"},
    {"shua", u8"\u5237"},
    {"shuai", u8"\u5e05|\u7529|\u6454"},
    {"shuan", u8"\u6813|\u62f4"},
    {"shuang", u8"\u53cc|\u723d|\u971c"},
    {"shui", u8"\u6c34|\u8c01|\u7a0e"},
    {"shun", u8"\u987a|\u77ac|\u542e"},
    {"shuo", u8"\u8bf4|\u7855|\u70c1"},
    {"si", u8"\u56db|\u601d|\u79c1|\u6b7b|\u53f8|\u4f3c"},
    {"song", u8"\u9001|\u677e|\u5b8b|\u9882"},
    {"sou", u8"\u641c|\u8258|\u55fd"},
    {"su", u8"\u901f|\u82cf|\u7d20|\u8bc9|\u5bbf"},
    {"suan", u8"\u7b97|\u9178"},
    {"sui", u8"\u968f|\u5c81|\u788e|\u867d|\u9042"},
    {"sun", u8"\u5b59|\u635f|\u7b0b"},
    {"suo", u8"\u6240|\u9501|\u7d22|\u7f29"},
    {"ta", u8"\u4ed6|\u5979|\u5b83|\u5854|\u8e0f"},
    {"tai", u8"\u592a|\u53f0|\u6001|\u62ac|\u6cf0"},
    {"tan", u8"\u8c08|\u5f39|\u63a2|\u5766|\u53f9|\u575b"},
    {"tang", u8"\u5510|\u7cd6|\u5802|\u6c64|\u8eba"},
    {"tao", u8"\u5957|\u6dd8|\u6843|\u8ba8|\u9676"},
    {"te", u8"\u7279|\u5fd2"},
    {"teng", u8"\u817e|\u75bc|\u85e4"},
    {"ti", u8"\u63d0|\u9898|\u4f53|\u66ff|\u8e22"},
    {"tian", u8"\u5929|\u7530|\u586b|\u751c|\u6dfb"},
    {"tiao", u8"\u6761|\u8c03|\u8df3|\u6311"},
    {"tie", u8"\u94c1|\u8d34|\u5e16"},
    {"ting", u8"\u542c|\u505c|\u5385|\u633a"},
    {"tong", u8"\u540c|\u901a|\u7edf|\u75db|\u7ae5|\u6876"},
    {"tou", u8"\u5934|\u6295|\u5077"},
    {"tu", u8"\u56fe|\u571f|\u7a81|\u5154|\u9014|\u5f92"},
    {"tuan", u8"\u56e2|\u6e4d"},
    {"tui", u8"\u63a8|\u9000|\u817f|\u892a"},
    {"tun", u8"\u541e|\u5c6f|\u81c0"},
    {"tuo", u8"\u6258|\u62d6|\u8131|\u62d3|\u59a5|\u9a6e"},
    {"wa", u8"\u54c7|\u5a03|\u74e6|\u86d9"},
    {"wai", u8"\u5916|\u6b6a"},
    {"wan", u8"\u5b8c|\u4e07|\u665a|\u6e7e|\u73a9|\u5f2f"},
    {"wang", u8"\u738b|\u7f51|\u671b|\u5f80|\u5fd8"},
    {"wei", u8"\u4e3a|\u4f4d|\u672a|\u59d4|\u7ef4|\u536b|\u5fae|\u5473"},
    {"wen", u8"\u6587|\u95ee|\u95fb|\u7a33|\u6e29"},
    {"weng", u8"\u7fc1|\u55e1"},
    {"wo", u8"\u6211|\u7a9d|\u63e1|\u6c83"},
    {"wu", u8"\u65e0|\u4e94|\u7269|\u52a1|\u8bef|\u821e|\u5c4b"},
    {"xi", u8"\u897f|\u559c|\u7cfb|\u7ec6|\u5e0c|\u4e60|\u606f|\u6d17"},
    {"xia", u8"\u4e0b|\u590f|\u5413|\u971e|\u8f96|\u4fa0"},
    {"xian", u8"\u5148|\u73b0|\u7ebf|\u53bf|\u9669|\u663e|\u9650|\u732e|\u5acc"},
    {"xiang", u8"\u60f3|\u5411|\u50cf|\u76f8|\u8c61|\u9879|\u4e61"},
    {"xiao", u8"\u5c0f|\u7b11|\u6821|\u6653|\u8096|\u6d88|\u6548"},
    {"xie", u8"\u5199|\u8c22|\u4e9b|\u978b|\u534f|\u6cc4|\u643a"},
    {"xin", u8"\u65b0|\u5fc3|\u4fe1|\u6b23|\u8f9b"},
    {"xing", u8"\u884c|\u6027|\u661f|\u5f62|\u9192|\u59d3"},
    {"xiong", u8"\u96c4|\u5144|\u80f8"},
    {"xiu", u8"\u4fee|\u4f11|\u79c0|\u8896|\u55c5"},
    {"xu", u8"\u9700|\u8bb8|\u7eed|\u5e8f|\u865a|\u5f90|\u987b"},
    {"xuan", u8"\u9009|\u5ba3|\u7384|\u65cb|\u60ac|\u55a7"},
    {"xue", u8"\u5b66|\u96ea|\u8840|\u7a74"},
    {"xun", u8"\u5bfb|\u8bad|\u8baf|\u8fc5|\u5faa"},
    {"ya", u8"\u5440|\u538b|\u4e9a|\u7259|\u82bd|\u96c5"},
    {"yan", u8"\u7814|\u8a00|\u773c|\u70df|\u5ef6|\u6f14|\u4e25|\u9a8c|\u8273"},
    {"yang", u8"\u6837|\u517b|\u9633|\u626c|\u6d0b|\u7f8a|\u6768"},
    {"yao", u8"\u8981|\u6447|\u836f|\u9065|\u8170|\u54ac|\u9080"},
    {"ye", u8"\u4e5f|\u591c|\u4e1a|\u53f6|\u7237|\u91ce"},
    {"yi", u8"\u4e00|\u4ee5|\u5df2|\u8863|\u6613|\u4e49|\u610f|\u8bae|\u533b"},
    {"yin", u8"\u56e0|\u97f3|\u5f15|\u94f6|\u5370|\u996e|\u9634"},
    {"ying", u8"\u5e94|\u82f1|\u5f71|\u8425|\u8fce|\u786c|\u8d62"},
    {"yo", u8"\u54df"},
    {"yong", u8"\u7528|\u6c38|\u52c7|\u62e5|\u6cf3|\u6d8c"},
    {"you", u8"\u6709|\u53c8|\u53cb|\u6e38|\u53f3|\u4f18|\u90ae|\u7531"},
    {"yu", u8"\u4e8e|\u4e0e|\u8bed|\u4f59|\u96e8|\u9c7c|\u7389|\u9047|\u80b2"},
    {"yuan", u8"\u5143|\u8fdc|\u9662|\u613f|\u539f|\u56ed|\u5706"},
    {"yue", u8"\u6708|\u8d8a|\u7ea6|\u9605|\u60a6"},
    {"yun", u8"\u4e91|\u8fd0|\u5141|\u6655|\u5b55"},
    {"za", u8"\u6742|\u548b"},
    {"zai", u8"\u5728|\u518d|\u8f7d|\u4ed4"},
    {"zan", u8"\u8d5e|\u6682|\u54b1|\u6512"},
    {"zang", u8"\u85cf|\u810f|\u846c"},
    {"zao", u8"\u65e9|\u9020|\u906d|\u7cdf|\u71e5"},
    {"ze", u8"\u5219|\u8d23|\u6cfd|\u62e9"},
    {"zei", u8"\u8d3c"},
    {"zen", u8"\u600e"},
    {"zeng", u8"\u589e|\u66fe|\u8d60"},
    {"zha", u8"\u70b8|\u624e|\u95f8|\u6e23|\u8bc8"},
    {"zhai", u8"\u5b85|\u503a|\u6458|\u5be8|\u658b"},
    {"zhan", u8"\u6218|\u7ad9|\u5360|\u5c55|\u65a9|\u6cbe"},
    {"zhang", u8"\u5f20|\u7ae0|\u638c|\u957f|\u6da8|\u5e10"},
    {"zhao", u8"\u627e|\u7167|\u62db|\u8d75|\u671d"},
    {"zhe", u8"\u8fd9|\u7740|\u8005|\u6298|\u906e|\u54f2"},
    {"zhen", u8"\u771f|\u9488|\u9635|\u9707|\u9547|\u6795"},
    {"zheng", u8"\u6b63|\u6574|\u653f|\u8bc1|\u4e89|\u90d1"},
    {"zhi", u8"\u53ea|\u4e4b|\u77e5|\u76f4|\u81f3|\u5fd7|\u6b62|\u6307|\u7eb8"},
    {"zhong", u8"\u4e2d|\u7ec8|\u949f|\u5fe0|\u4f17|\u91cd"},
    {"zhou", u8"\u5468|\u5dde|\u6d32|\u7ca5|\u8f74"},
    {"zhu", u8"\u4e3b|\u4f4f|\u6ce8|\u52a9|\u6731|\u795d|\u8bf8"},
    {"zhua", u8"\u6293"},
    {"zhuai", u8"\u62fd"},
    {"zhuan", u8"\u8f6c|\u4e13|\u7816|\u8d5a|\u4f20"},
    {"zhuang", u8"\u88c5|\u5e84|\u72b6|\u649e|\u58ee"},
    {"zhui", u8"\u8ffd|\u5760|\u7f00|\u8d58"},
    {"zhun", u8"\u51c6|\u8c06"},
    {"zhuo", u8"\u684c|\u6349|\u707c|\u5353|\u62d9"},
    {"zi", u8"\u5b50|\u81ea|\u5b57|\u8d44|\u7d2b|\u59ff"},
    {"zong", u8"\u603b|\u5b97|\u7eb5|\u68d5|\u8e2a"},
    {"zou", u8"\u8d70|\u594f|\u90b9|\u63cd"},
    {"zu", u8"\u7ec4|\u8db3|\u65cf|\u79df|\u7956"},
    {"zuan", u8"\u94bb|\u8d5a"},
    {"zui", u8"\u6700|\u7f6a|\u5634"},
    {"zun", u8"\u5c0a|\u9075|\u6a3d"},
    {"zuo", u8"\u505a|\u4f5c|\u5750|\u5de6|\u6628"},
    {"nihao", u8"\u4f60\u597d"},
    {"xiexie", u8"\u8c22\u8c22"},
    {"zaijian", u8"\u518d\u89c1"},
    {"zhongguo", u8"\u4e2d\u56fd"},
    {"women", u8"\u6211\u4eec"},
    {"nimen", u8"\u4f60\u4eec"},
    {"tamen", u8"\u4ed6\u4eec"},
    {"haode", u8"\u597d\u7684"},
    {"meiyou", u8"\u6ca1\u6709"},
    {"mingzi", u8"\u540d\u5b57"},
    {"pengyou", u8"\u670b\u53cb"},
    {"laoshi", u8"\u8001\u5e08"},
    {"xuesheng", u8"\u5b66\u751f"},
    {"jintian", u8"\u4eca\u5929"},
    {"mingtian", u8"\u660e\u5929"},
    {"zuotian", u8"\u6628\u5929"},
    {"haoma", u8"\u53f7\u7801"},
    {"shouji", u8"\u624b\u673a"},
    {"dianhua", u8"\u7535\u8bdd"},
    {"gongsi", u8"\u516c\u53f8"},
    {"zaoshang", u8"\u65e9\u4e0a"},
    {"wanshang", u8"\u665a\u4e0a"},
    {"xianzai", u8"\u73b0\u5728"},
    {"yihou", u8"\u4ee5\u540e"},
    {"yixia", u8"\u4e00\u4e0b"},
    {"yidian", u8"\u4e00\u70b9"},
    {"yixie", u8"\u4e00\u4e9b"},
    {"zenme", u8"\u600e\u4e48"},
    {"weishenme", u8"\u4e3a\u4ec0\u4e48"},
    {"shenme", u8"\u4ec0\u4e48"},
    {"keyi", u8"\u53ef\u4ee5"},
    {"bukeyi", u8"\u4e0d\u53ef\u4ee5"},
    {"meishi", u8"\u6ca1\u4e8b"},
    {"meiguanxi", u8"\u6ca1\u5173\u7cfb"},
    {"duibuqi", u8"\u5bf9\u4e0d\u8d77"},
    {"baoqian", u8"\u62b1\u6b49"},
    {"qingwen", u8"\u8bf7\u95ee"},
    {"mingbai", u8"\u660e\u767d"},
    {"zhidao", u8"\u77e5\u9053"},
    {"xiangxin", u8"\u76f8\u4fe1"},
    {"xiangtong", u8"\u76f8\u540c"},
    {"xuanze", u8"\u9009\u62e9"},
    {"yonghu", u8"\u7528\u6237"},
    {"mima", u8"\u5bc6\u7801"},
    {"zhanghao", u8"\u8d26\u53f7"},
    {"denglu", u8"\u767b\u5f55"},
    {"tuichu", u8"\u9000\u51fa"},
    {"zaixian", u8"\u5728\u7ebf"},
    {"lixian", u8"\u79bb\u7ebf"},
    {"wenjian", u8"\u6587\u4ef6"},
    {"tupian", u8"\u56fe\u7247"},
    {"yuyin", u8"\u8bed\u97f3"},
    {"shipin", u8"\u89c6\u9891"},
    {"xiaoxi", u8"\u6d88\u606f"},
    {"fasong", u8"\u53d1\u9001"},
    {"jieshou", u8"\u63a5\u6536"},
    {"lianjie", u8"\u8fde\u63a5"},
    {"shurufa", u8"\u8f93\u5165\u6cd5"},
    {"lianxiang", u8"\u8054\u60f3"},
    {"qiehuan", u8"\u5207\u6362"},
    {"zhongwen", u8"\u4e2d\u6587"},
    {"yingwen", u8"\u82f1\u6587"},
    {"tishi", u8"\u63d0\u793a"},
    {"tongzhi", u8"\u901a\u77e5"},
    {"anquan", u8"\u5b89\u5168"},
    {"jiami", u8"\u52a0\u5bc6"},
    {"jiemi", u8"\u89e3\u5bc6"},
    {"yanzheng", u8"\u9a8c\u8bc1"},
    {"chenggong", u8"\u6210\u529f"},
    {"shibai", u8"\u5931\u8d25"},
    {"gengxin", u8"\u66f4\u65b0"},
    {"chongshi", u8"\u91cd\u8bd5"},
    {"peizhi", u8"\u914d\u7f6e"},
    {"xiangmu", u8"\u9879\u76ee"},
    {"haoyou", u8"\u597d\u53cb"},
    {"qunliao", u8"\u7fa4\u804a"},
    {"qunzu", u8"\u7fa4\u7ec4"},
    {"shebei", u8"\u8bbe\u5907"},
    {"zhuangtai", u8"\u72b6\u6001"},
    {"banben", u8"\u7248\u672c"},
    {"xiazai", u8"\u4e0b\u8f7d"},
    {"shangchuan", u8"\u4e0a\u4f20"},
    {"cunchu", u8"\u5b58\u50a8"},
    {"duqu", u8"\u8bfb\u53d6"},
    {"baocun", u8"\u4fdd\u5b58"},
    {"queren", u8"\u786e\u8ba4"},
    {"quxiao", u8"\u53d6\u6d88"},
    {"shezhi", u8"\u8bbe\u7f6e"},
    {"sousuo", u8"\u641c\u7d22"},
    {"xieyi", u8"\u534f\u8bae"},
    {"fuwu", u8"\u670d\u52a1"},
    {"jianli", u8"\u5efa\u7acb"},
    {"zhengchang", u8"\u6b63\u5e38"},
    {"cuowu", u8"\u9519\u8bef"},
};

QStringList SplitCandidates(const char *raw) {
    QStringList list;
    if (!raw || !*raw) {
        return list;
    }
    QString current;
    const QByteArray bytes(raw);
    const QString text = QString::fromUtf8(bytes);
    for (QChar ch : text) {
        if (ch == QChar('|')) {
            if (!current.isEmpty()) {
                list.push_back(current);
            }
            current.clear();
        } else {
            current.append(ch);
        }
    }
    if (!current.isEmpty()) {
        list.push_back(current);
    }
    return list;
}

struct PinyinIndex {
    QHash<QString, QStringList> dict;
    QVector<QString> keys;
    QSet<QString> keySet;
    int maxKeyLength{0};
};

bool LoadPinyinDictFromResource(QHash<QString, QStringList> &dict, int *maxKeyLength) {
    QFile file(QString::fromLatin1(kPinyinDictResourcePath));
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif
    int maxLen = 0;
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.isEmpty() || line.startsWith(QChar('#'))) {
            continue;
        }
        const int tab = line.indexOf(QChar('\t'));
        if (tab <= 0) {
            continue;
        }
        const QString key = line.left(tab).trimmed();
        const QString phrase = line.mid(tab + 1).trimmed();
        if (key.isEmpty() || phrase.isEmpty()) {
            continue;
        }
        auto &list = dict[key];
        if (list.size() >= kMaxPinyinCandidatesPerKey || list.contains(phrase)) {
            continue;
        }
        list.push_back(phrase);
        maxLen = qMax(maxLen, key.size());
    }
    if (maxKeyLength) {
        *maxKeyLength = maxLen;
    }
    return !dict.isEmpty();
}

PinyinIndex BuildPinyinIndex() {
    PinyinIndex index;
    if (!LoadPinyinDictFromResource(index.dict, &index.maxKeyLength)) {
        for (const auto &entry : kPinyinDict) {
            const QString key = QString::fromLatin1(entry.key);
            index.dict.insert(key, SplitCandidates(entry.candidates));
            index.maxKeyLength = qMax(index.maxKeyLength, key.size());
        }
    }
    index.keys.reserve(index.dict.size());
    for (auto it = index.dict.constBegin(); it != index.dict.constEnd(); ++it) {
        index.keys.push_back(it.key());
        index.keySet.insert(it.key());
        index.maxKeyLength = qMax(index.maxKeyLength, it.key().size());
    }
    std::sort(index.keys.begin(), index.keys.end());
    return index;
}

const PinyinIndex &GetPinyinIndex() {
    static PinyinIndex index = BuildPinyinIndex();
    return index;
}

const QHash<QString, QStringList> &PinyinDict() {
    return GetPinyinIndex().dict;
}

const QVector<QString> &PinyinKeys() {
    return GetPinyinIndex().keys;
}

const QSet<QString> &PinyinKeySet() {
    return GetPinyinIndex().keySet;
}

int PinyinMaxKeyLength() {
    return GetPinyinIndex().maxKeyLength;
}

struct EnglishEntry {
    const char *word;
};

static const EnglishEntry kEnglishDict[] = {
    {"about"}, {"above"}, {"accept"}, {"access"}, {"account"}, {"action"},
    {"active"}, {"activity"}, {"add"}, {"address"}, {"admin"}, {"after"},
    {"again"}, {"agent"}, {"agree"}, {"air"}, {"all"}, {"allow"},
    {"almost"}, {"along"}, {"already"}, {"also"}, {"always"}, {"amount"},
    {"and"}, {"another"}, {"answer"}, {"any"}, {"anyone"}, {"anything"},
    {"app"}, {"apply"}, {"are"}, {"around"}, {"ask"}, {"attach"},
    {"available"}, {"away"}, {"back"}, {"bad"}, {"base"}, {"be"},
    {"because"}, {"become"}, {"before"}, {"begin"}, {"behind"}, {"below"},
    {"best"}, {"better"}, {"between"}, {"big"}, {"block"}, {"both"},
    {"build"}, {"busy"}, {"button"}, {"buy"}, {"by"}, {"call"},
    {"can"}, {"cancel"}, {"cannot"}, {"card"}, {"care"}, {"case"},
    {"change"}, {"chat"}, {"check"}, {"choose"}, {"clear"}, {"click"},
    {"close"}, {"code"}, {"color"}, {"come"}, {"comment"}, {"connect"},
    {"contact"}, {"content"}, {"continue"}, {"copy"}, {"core"}, {"could"},
    {"create"}, {"current"}, {"customer"}, {"data"}, {"date"}, {"day"},
    {"debug"}, {"default"}, {"delete"}, {"deny"}, {"detail"}, {"device"},
    {"did"}, {"different"}, {"direct"}, {"disable"}, {"done"}, {"download"},
    {"draw"}, {"drive"}, {"early"}, {"easy"}, {"edit"}, {"effect"},
    {"emoji"}, {"enable"}, {"end"}, {"enter"}, {"error"}, {"even"},
    {"event"}, {"every"}, {"example"}, {"fail"}, {"false"}, {"fast"},
    {"feature"}, {"file"}, {"find"}, {"finish"}, {"first"}, {"fix"},
    {"focus"}, {"follow"}, {"for"}, {"force"}, {"format"}, {"forward"},
    {"found"}, {"from"}, {"full"}, {"function"}, {"get"}, {"give"},
    {"global"}, {"go"}, {"good"}, {"group"}, {"grow"}, {"great"},
    {"hand"}, {"have"}, {"health"}, {"help"}, {"here"}, {"hide"},
    {"high"}, {"history"}, {"home"}, {"how"}, {"icon"}, {"idea"},
    {"idle"}, {"if"}, {"ignore"}, {"import"}, {"in"}, {"include"},
    {"info"}, {"input"}, {"install"}, {"into"}, {"invalid"}, {"is"},
    {"item"}, {"join"}, {"just"}, {"keep"}, {"key"}, {"kind"},
    {"know"}, {"label"}, {"last"}, {"later"}, {"leave"}, {"left"},
    {"less"}, {"let"}, {"level"}, {"like"}, {"link"}, {"list"},
    {"load"}, {"local"}, {"lock"}, {"log"}, {"login"}, {"logout"},
    {"long"}, {"look"}, {"loss"}, {"low"}, {"main"}, {"make"},
    {"manage"}, {"many"}, {"map"}, {"maybe"}, {"me"}, {"mean"},
    {"meet"}, {"message"}, {"method"}, {"mode"}, {"more"}, {"most"},
    {"move"}, {"msg"}, {"much"}, {"name"}, {"need"}, {"new"},
    {"next"}, {"no"}, {"normal"}, {"note"}, {"now"}, {"object"},
    {"of"}, {"off"}, {"offline"}, {"ok"}, {"okay"}, {"on"},
    {"once"}, {"online"}, {"only"}, {"open"}, {"option"}, {"or"},
    {"order"}, {"other"}, {"out"}, {"over"}, {"page"}, {"pair"},
    {"panel"}, {"paper"}, {"parent"}, {"parse"}, {"part"}, {"paste"},
    {"path"}, {"pause"}, {"peer"}, {"people"}, {"period"}, {"phone"},
    {"photo"}, {"pin"}, {"ping"}, {"place"}, {"play"}, {"please"},
    {"point"}, {"port"}, {"post"}, {"press"}, {"preview"}, {"print"},
    {"private"}, {"profile"}, {"progress"}, {"project"}, {"prompt"}, {"public"},
    {"pull"}, {"push"}, {"quick"}, {"quit"}, {"read"}, {"ready"},
    {"real"}, {"reason"}, {"receive"}, {"recent"}, {"record"}, {"red"},
    {"refresh"}, {"refuse"}, {"register"}, {"reload"}, {"remove"}, {"rename"},
    {"reply"}, {"report"}, {"request"}, {"reset"}, {"retry"}, {"return"},
    {"right"}, {"role"}, {"root"}, {"run"}, {"safe"}, {"same"},
    {"save"}, {"scan"}, {"screen"}, {"script"}, {"search"}, {"secure"},
    {"select"}, {"send"}, {"server"}, {"service"}, {"session"}, {"set"},
    {"setting"}, {"share"}, {"show"}, {"sign"}, {"silent"}, {"simple"},
    {"since"}, {"size"}, {"slow"}, {"small"}, {"soft"}, {"some"},
    {"sort"}, {"sound"}, {"source"}, {"space"}, {"start"}, {"state"},
    {"status"}, {"stop"}, {"store"}, {"string"}, {"strong"}, {"system"},
    {"take"}, {"task"}, {"team"}, {"test"}, {"text"}, {"thank"},
    {"thanks"}, {"then"}, {"time"}, {"timer"}, {"title"}, {"today"},
    {"tomorrow"}, {"tool"}, {"topic"}, {"total"}, {"touch"}, {"try"},
    {"type"}, {"under"}, {"undo"}, {"unit"}, {"unlock"}, {"update"},
    {"upload"}, {"upper"}, {"use"}, {"user"}, {"value"}, {"view"},
    {"video"}, {"voice"}, {"volume"}, {"wait"}, {"want"}, {"warn"},
    {"way"}, {"we"}, {"welcome"}, {"what"}, {"when"}, {"where"},
    {"which"}, {"who"}, {"why"}, {"wide"}, {"will"}, {"window"},
    {"with"}, {"without"}, {"word"}, {"work"}, {"wrong"}, {"yes"},
    {"you"}, {"your"},
};

struct EnglishIndex {
    QVector<QString> words;
    QHash<QChar, QVector<QString>> buckets;
};

bool LoadEnglishDictFromResource(QVector<QString> &dict) {
    QFile file(QString::fromLatin1(kEnglishDictResourcePath));
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QChar('#'))) {
            continue;
        }
        line = line.toLower();
        dict.push_back(line);
    }
    return !dict.isEmpty();
}

EnglishIndex BuildEnglishIndex() {
    EnglishIndex index;
    if (!LoadEnglishDictFromResource(index.words)) {
        index.words.reserve(static_cast<int>(sizeof(kEnglishDict) / sizeof(kEnglishDict[0])));
        for (const auto &entry : kEnglishDict) {
            index.words.push_back(QString::fromLatin1(entry.word));
        }
    }
    index.buckets.reserve(32);
    for (const auto &word : index.words) {
        if (word.isEmpty()) {
            continue;
        }
        index.buckets[word.at(0)].push_back(word);
    }
    return index;
}

const EnglishIndex &GetEnglishIndex() {
    static EnglishIndex index = BuildEnglishIndex();
    return index;
}

bool IsEnglishLetter(QChar ch) {
    return ch.isLetter() && ch.unicode() < 128;
}

QString ApplyEnglishCase(const QString &word, const QString &prefix) {
    if (prefix.isEmpty()) {
        return word;
    }
    const QString upper = prefix.toUpper();
    const QString lower = prefix.toLower();
    if (prefix == upper) {
        return word.toUpper();
    }
    if (prefix.at(0).isUpper() && prefix.mid(1) == lower.mid(1)) {
        QString out = word;
        out[0] = out.at(0).toUpper();
        return out;
    }
    return word;
}

QString SegmentFallback(const QString &pinyin) {
    if (pinyin.isEmpty()) {
        return {};
    }
    const int n = pinyin.size();
    QVector<int> score(n + 1, -1);
    QVector<int> prev(n + 1, -1);
    QVector<QString> prevKey(n + 1);
    score[0] = 0;
    const auto &dict = PinyinDict();
    const auto &keySet = PinyinKeySet();
    const int maxLen = PinyinMaxKeyLength();
    if (maxLen <= 0) {
        return {};
    }
    for (int i = 0; i < n; ++i) {
        if (score[i] < 0) {
            continue;
        }
        const int limit = qMin(maxLen, n - i);
        for (int len = 1; len <= limit; ++len) {
            const QString key = pinyin.mid(i, len);
            if (!keySet.contains(key)) {
                continue;
            }
            const int j = i + len;
            const int nextScore = score[i] + len * 2 - 1;
            if (nextScore > score[j]) {
                score[j] = nextScore;
                prev[j] = i;
                prevKey[j] = key;
            }
        }
    }
    if (score[n] < 0) {
        return {};
    }
    QStringList chunks;
    int cur = n;
    while (cur > 0 && prev[cur] >= 0) {
        const QString key = prevKey[cur];
        const auto it = dict.constFind(key);
        if (it != dict.constEnd() && !it.value().isEmpty()) {
            chunks.push_front(it.value().front());
        }
        cur = prev[cur];
    }
    return chunks.join(QString());
}

QStringList BuildCandidates(const QString &pinyin) {
    const auto &dict = PinyinDict();
    QStringList list;
    const auto it = dict.constFind(pinyin);
    if (it != dict.constEnd()) {
        list = it.value();
    }
    const QString fallback = SegmentFallback(pinyin);
    if (!fallback.isEmpty() && !list.contains(fallback)) {
        list.push_back(fallback);
    }
    if (!pinyin.isEmpty() && list.size() < 5) {
        const auto &keys = PinyinKeys();
        auto it = std::lower_bound(keys.begin(), keys.end(), pinyin);
        for (; it != keys.end(); ++it) {
            if (!it->startsWith(pinyin)) {
                break;
            }
            if (*it == pinyin) {
                continue;
            }
            const auto hit = dict.constFind(*it);
            if (hit == dict.constEnd() || hit.value().isEmpty()) {
                continue;
            }
            const QString &cand = hit.value().front();
            if (!list.contains(cand)) {
                list.push_back(cand);
            }
            if (list.size() >= 5) {
                break;
            }
        }
    }
    if (list.isEmpty()) {
        list.push_back(pinyin);
    }
    return list;
}

QStringList BuildEnglishCandidates(const QString &prefix) {
    QStringList list;
    if (prefix.isEmpty() || prefix.size() < 2) {
        return list;
    }
    const QString lower = prefix.toLower();
    const auto &index = GetEnglishIndex();
    const auto it = index.buckets.constFind(lower.at(0));
    if (it == index.buckets.constEnd()) {
        return list;
    }
    for (const auto &word : it.value()) {
        if (!word.startsWith(lower)) {
            continue;
        }
        if (word == lower) {
            continue;
        }
        list.push_back(ApplyEnglishCase(word, prefix));
        if (list.size() >= 5) {
            break;
        }
    }
    return list;
}

class CandidateLabel : public QLabel {
public:
    explicit CandidateLabel(QWidget *parent = nullptr) : QLabel(parent) {
        setTextFormat(Qt::RichText);
    }
};

}  // namespace

class ChatInputEdit::CandidatePopup : public QFrame {
public:
    explicit CandidatePopup(QWidget *parent = nullptr) : QFrame(parent) {
        setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_ShowWithoutActivating, true);
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setStyleSheet(QStringLiteral(
            "QFrame { background: %1; border: 1px solid %2; border-radius: 8px; }"
            "QLabel { color: %3; font-size: 11px; padding: 6px 8px; }")
                          .arg(Theme::uiPanelBg().name(),
                               Theme::uiBorder().name(),
                               Theme::uiTextMain().name()));
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        label_ = new CandidateLabel(this);
        layout->addWidget(label_);
    }

    void setCandidates(const QString &pinyin, const QStringList &cands, int selected) {
        if (!label_) {
            return;
        }
        QStringList parts;
        const int maxCount = qMin(5, cands.size());
        for (int i = 0; i < maxCount; ++i) {
            const QString entry =
                QStringLiteral("%1.%2").arg(i + 1).arg(cands.at(i));
            if (i == selected) {
                parts.push_back(QStringLiteral("<span style=\"color:%1;\">%2</span>")
                                    .arg(Theme::uiAccentBlue().name(),
                                         entry.toHtmlEscaped()));
            } else {
                parts.push_back(entry.toHtmlEscaped());
            }
        }
        const QString head = pinyin.isEmpty() ? QString() : pinyin.toHtmlEscaped();
        const QString body = parts.join(QStringLiteral("  "));
        if (head.isEmpty()) {
            label_->setText(body);
        } else {
            label_->setText(head + QStringLiteral("  ") + body);
        }
        adjustSize();
    }

private:
    CandidateLabel *label_{nullptr};
};

ChatInputEdit::ChatInputEdit(QWidget *parent) : QPlainTextEdit(parent) {
    gInputEdits.insert(this);
    inputMode_ = gInputMode;
    setAttribute(Qt::WA_InputMethodEnabled, !imeEnabled_);
    connect(this, &QPlainTextEdit::textChanged, this, [this]() {
        if (imeEnabled_ && inputMode_ == InputMode::English) {
            updateEnglishSuggestions();
        }
    });
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, [this]() {
        if (imeEnabled_ && inputMode_ == InputMode::English) {
            updateEnglishSuggestions();
        }
    });
}

ChatInputEdit::~ChatInputEdit() {
    gInputEdits.remove(this);
}

bool ChatInputEdit::isComposing() const {
    return composing_;
}

bool ChatInputEdit::isNativeComposing() const {
    return nativeComposing_;
}

bool ChatInputEdit::imeEnabled() const {
    return imeEnabled_;
}

ChatInputEdit::InputMode ChatInputEdit::inputMode() const {
    return inputMode_;
}

bool ChatInputEdit::isChineseMode() const {
    return inputMode_ == InputMode::Chinese;
}

void ChatInputEdit::setInputMode(InputMode mode) {
    if (gInputMode == mode) {
        return;
    }
    gInputMode = mode;
    for (auto *edit : gInputEdits) {
        if (edit) {
            edit->applyInputMode(mode);
        }
    }
}

void ChatInputEdit::setImeEnabled(bool enabled) {
    if (imeEnabled_ == enabled) {
        return;
    }
    imeEnabled_ = enabled;
    nativeComposing_ = false;
    setAttribute(Qt::WA_InputMethodEnabled, !imeEnabled_);
    cancelComposition(true);
    cancelEnglishSuggestions();
}

bool ChatInputEdit::commitDefaultCandidate() {
    if (!composing_) {
        return false;
    }
    commitCandidate(candidateIndex_);
    return true;
}

void ChatInputEdit::keyPressEvent(QKeyEvent *event) {
    if (!event) {
        QPlainTextEdit::keyPressEvent(event);
        return;
    }
    if (event->matches(QKeySequence::InsertLineSeparator)) {
        if (composing_) {
            commitCandidate(candidateIndex_);
            event->accept();
            return;
        }
        QPlainTextEdit::keyPressEvent(event);
        return;
    }
    if ((event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) ==
            (Qt::ControlModifier | Qt::ShiftModifier) &&
        event->key() == Qt::Key_Space) {
        handleToggleIme();
        event->accept();
        return;
    }
    if (imeEnabled_) {
        if (event->key() == Qt::Key_Shift && !event->isAutoRepeat()) {
            shiftPressed_ = true;
            shiftUsed_ = false;
            event->accept();
            return;
        }
        if (shiftPressed_ && event->key() != Qt::Key_Shift) {
            shiftUsed_ = true;
        }
    }
    if (!imeEnabled_) {
        QPlainTextEdit::keyPressEvent(event);
        return;
    }
    if (inputMode_ == InputMode::Chinese) {
        if (handleCompositionKey(event)) {
            return;
        }
        QPlainTextEdit::keyPressEvent(event);
        return;
    }
    if (handleEnglishSuggestionKey(event)) {
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
    updateEnglishSuggestions();
}

void ChatInputEdit::keyReleaseEvent(QKeyEvent *event) {
    if (!event) {
        QPlainTextEdit::keyReleaseEvent(event);
        return;
    }
    if (imeEnabled_ && event->key() == Qt::Key_Shift && !event->isAutoRepeat()) {
        const bool shouldToggle = shiftPressed_ && !shiftUsed_;
        shiftPressed_ = false;
        shiftUsed_ = false;
        if (shouldToggle) {
            toggleInputMode();
            event->accept();
            return;
        }
    }
    QPlainTextEdit::keyReleaseEvent(event);
}

void ChatInputEdit::inputMethodEvent(QInputMethodEvent *event) {
    nativeComposing_ = (event && !event->preeditString().isEmpty());
    QPlainTextEdit::inputMethodEvent(event);
    if (!event || event->preeditString().isEmpty()) {
        nativeComposing_ = false;
    }
}

void ChatInputEdit::focusOutEvent(QFocusEvent *event) {
    nativeComposing_ = false;
    cancelComposition(true);
    cancelEnglishSuggestions();
    shiftPressed_ = false;
    shiftUsed_ = false;
    QPlainTextEdit::focusOutEvent(event);
}

void ChatInputEdit::mousePressEvent(QMouseEvent *event) {
    cancelComposition(true);
    cancelEnglishSuggestions();
    QPlainTextEdit::mousePressEvent(event);
}

void ChatInputEdit::resizeEvent(QResizeEvent *event) {
    QPlainTextEdit::resizeEvent(event);
    updatePopupPosition();
}

void ChatInputEdit::handleToggleIme() {
    setImeEnabled(!imeEnabled_);
    hidePopup();
}

void ChatInputEdit::toggleInputMode() {
    setInputMode(inputMode_ == InputMode::Chinese ? InputMode::English : InputMode::Chinese);
}

void ChatInputEdit::applyInputMode(InputMode mode) {
    if (inputMode_ == mode) {
        return;
    }
    inputMode_ = mode;
    cancelComposition(true);
    cancelEnglishSuggestions();
    if (imeEnabled_ && inputMode_ == InputMode::English) {
        updateEnglishSuggestions();
    }
    emit inputModeChanged(inputMode_ == InputMode::Chinese);
}

bool ChatInputEdit::handleCompositionKey(QKeyEvent *event) {
    const int key = event->key();
    const Qt::KeyboardModifiers mods = event->modifiers();
    if (mods.testFlag(Qt::ControlModifier) || mods.testFlag(Qt::AltModifier) ||
        mods.testFlag(Qt::MetaModifier)) {
        return false;
    }
    const QString text = event->text();
    if (text.size() == 1) {
        const QChar ch = text.at(0);
        if (ch.isLetter()) {
            startComposition(QString(ch).toLower());
            event->accept();
            return true;
        }
    }
    if (!composing_) {
        return false;
    }
    if (key == Qt::Key_Backspace) {
        if (!composition_.isEmpty()) {
            composition_.chop(1);
            if (composition_.isEmpty()) {
                cancelComposition(false);
            } else {
                updateCompositionText();
                updateCandidates();
            }
        } else {
            cancelComposition(false);
        }
        event->accept();
        return true;
    }
    if (key == Qt::Key_Space || key == Qt::Key_Return || key == Qt::Key_Enter) {
        commitCandidate(candidateIndex_);
        event->accept();
        return true;
    }
    if (key >= Qt::Key_1 && key <= Qt::Key_5) {
        const int index = key - Qt::Key_1;
        commitCandidate(index);
        event->accept();
        return true;
    }
    if (key == Qt::Key_Left) {
        candidateIndex_ = qMax(0, candidateIndex_ - 1);
        updateCandidates();
        event->accept();
        return true;
    }
    if (key == Qt::Key_Right) {
        candidateIndex_ = qMin(candidateIndex_ + 1, candidates_.size() - 1);
        updateCandidates();
        event->accept();
        return true;
    }
    if (key == Qt::Key_Escape) {
        cancelComposition(false);
        event->accept();
        return true;
    }
    if (text.size() == 1 && !text.at(0).isLetter() && !text.at(0).isSpace()) {
        commitCandidate(candidateIndex_);
        QPlainTextEdit::keyPressEvent(event);
        return true;
    }
    return true;
}

bool ChatInputEdit::handleEnglishSuggestionKey(QKeyEvent *event) {
    if (!englishSuggesting_ || englishCandidates_.isEmpty()) {
        return false;
    }
    const int key = event->key();
    if (key == Qt::Key_Escape) {
        cancelEnglishSuggestions();
        event->accept();
        return true;
    }
    if (key == Qt::Key_Tab || key == Qt::Key_Backtab) {
        commitEnglishCandidate(englishCandidateIndex_);
        event->accept();
        return true;
    }
    if (key >= Qt::Key_1 && key <= Qt::Key_5) {
        const int index = key - Qt::Key_1;
        commitEnglishCandidate(index);
        event->accept();
        return true;
    }
    return false;
}

void ChatInputEdit::startComposition(const QString &ch) {
    if (!composing_) {
        composing_ = true;
        candidateIndex_ = 0;
        compStart_ = textCursor().position();
        compLength_ = 0;
        composition_.clear();
    }
    composition_ += ch;
    updateCompositionText();
    updateCandidates();
}

void ChatInputEdit::updateCompositionText() {
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    cursor.setPosition(compStart_);
    cursor.setPosition(compStart_ + compLength_, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.insertText(composition_);
    compLength_ = composition_.size();
    cursor.setPosition(compStart_);
    cursor.setPosition(compStart_ + compLength_, QTextCursor::KeepAnchor);
    setTextCursor(cursor);
    cursor.endEditBlock();
}

void ChatInputEdit::updateCandidates() {
    candidates_ = BuildCandidates(composition_);
    if (candidateIndex_ >= candidates_.size()) {
        candidateIndex_ = 0;
    }
    if (candidates_.size() == 1 && candidates_.front() != composition_) {
        commitCandidate(0);
        return;
    }
    showPopup();
    if (popup_) {
        popup_->setCandidates(composition_, candidates_, candidateIndex_);
    }
    updatePopupPosition();
}

void ChatInputEdit::updateEnglishSuggestions() {
    if (!imeEnabled_ || inputMode_ != InputMode::English) {
        cancelEnglishSuggestions();
        return;
    }
    if (composing_) {
        cancelEnglishSuggestions();
        return;
    }
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection()) {
        cancelEnglishSuggestions();
        return;
    }
    const QString blockText = cursor.block().text();
    const int pos = cursor.positionInBlock();
    if (pos <= 0 || pos > blockText.size()) {
        cancelEnglishSuggestions();
        return;
    }
    int start = pos;
    while (start > 0 && IsEnglishLetter(blockText.at(start - 1))) {
        --start;
    }
    const int len = pos - start;
    if (len < 2) {
        cancelEnglishSuggestions();
        return;
    }
    const QString prefix = blockText.mid(start, len);
    englishStart_ = cursor.position() - len;
    englishLength_ = len;
    QStringList next = BuildEnglishCandidates(prefix);
    if (next.isEmpty()) {
        cancelEnglishSuggestions();
        return;
    }
    const bool samePrefix = (englishPrefix_ == prefix);
    englishPrefix_ = prefix;
    englishCandidates_ = next;
    if (!samePrefix || englishCandidateIndex_ >= englishCandidates_.size()) {
        englishCandidateIndex_ = 0;
    }
    englishSuggesting_ = true;
    showPopup();
    if (popup_) {
        popup_->setCandidates(englishPrefix_, englishCandidates_, englishCandidateIndex_);
    }
    updatePopupPosition();
}

void ChatInputEdit::commitEnglishCandidate(int index) {
    if (!englishSuggesting_ || englishCandidates_.isEmpty()) {
        return;
    }
    const int safeIndex = qBound(0, index, englishCandidates_.size() - 1);
    const QString candidate = englishCandidates_.at(safeIndex);
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    cursor.setPosition(englishStart_);
    cursor.setPosition(englishStart_ + englishLength_, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.insertText(candidate);
    cursor.endEditBlock();
    setTextCursor(cursor);
    cancelEnglishSuggestions();
}

void ChatInputEdit::cancelEnglishSuggestions() {
    const bool wasShowing = englishSuggesting_;
    englishSuggesting_ = false;
    englishPrefix_.clear();
    englishCandidates_.clear();
    englishCandidateIndex_ = 0;
    englishStart_ = 0;
    englishLength_ = 0;
    if (wasShowing) {
        hidePopup();
    }
}

void ChatInputEdit::commitCandidate(int index) {
    if (!composing_) {
        return;
    }
    if (candidates_.isEmpty()) {
        cancelComposition(true);
        return;
    }
    const int safeIndex = qBound(0, index, candidates_.size() - 1);
    const QString candidate = candidates_.at(safeIndex);
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    cursor.setPosition(compStart_);
    cursor.setPosition(compStart_ + compLength_, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.insertText(candidate);
    cursor.endEditBlock();
    setTextCursor(cursor);
    composing_ = false;
    composition_.clear();
    candidates_.clear();
    compLength_ = 0;
    hidePopup();
}

void ChatInputEdit::cancelComposition(bool keepText) {
    if (!composing_) {
        hidePopup();
        return;
    }
    if (!keepText) {
        QTextCursor cursor = textCursor();
        cursor.beginEditBlock();
        cursor.setPosition(compStart_);
        cursor.setPosition(compStart_ + compLength_, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.endEditBlock();
        setTextCursor(cursor);
    } else {
        QTextCursor cursor = textCursor();
        cursor.setPosition(compStart_ + compLength_);
        setTextCursor(cursor);
    }
    composing_ = false;
    composition_.clear();
    candidates_.clear();
    compLength_ = 0;
    hidePopup();
}

void ChatInputEdit::ensurePopup() {
    if (!popup_) {
        popup_ = new CandidatePopup(this);
    }
}

void ChatInputEdit::showPopup() {
    ensurePopup();
    if (popup_ && !popup_->isVisible()) {
        popup_->show();
    }
}

void ChatInputEdit::hidePopup() {
    if (popup_) {
        popup_->hide();
    }
}

void ChatInputEdit::updatePopupPosition() {
    if (!popup_ || !popup_->isVisible()) {
        return;
    }
    const QRect cursor = cursorRect();
    QPoint global = mapToGlobal(QPoint(cursor.left(), cursor.top()));
    const QSize popupSize = popup_->sizeHint();
    global.setY(global.y() - popupSize.height() - 6);
    if (QScreen *screen = QGuiApplication::screenAt(global)) {
        const QRect bounds = screen->availableGeometry();
        if (global.y() < bounds.top()) {
            global = mapToGlobal(QPoint(cursor.left(), cursor.bottom() + 6));
        }
        if (global.x() + popupSize.width() > bounds.right()) {
            global.setX(bounds.right() - popupSize.width());
        }
        if (global.x() < bounds.left()) {
            global.setX(bounds.left());
        }
    }
    popup_->move(global);
}
