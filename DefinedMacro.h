/// 读取配置文件返回状态
#define  INIT_OK							  1		// 配置文件读取成功
#define  INIT_ERROR_COMMNETSETTINGS			  0		// 配置文件读取失败[CommNetSettings]
#define  INIT_ERROR_PATH         		     -1		// 配置文件读取失败[Path]
#define  INIT_ERROR_CAMERASETTINGS   	     -2		// 配置文件读取失败[CameraSettings]
#define  INIT_ERROR_DISPLAYSETTINGS  	     -3		// 配置文件读取失败[DisplaySettings]
#define  INIT_ERROR_TRACKSETTINGS            -4     // 配置文件读取失败[TrackSettings]
#define  INIT_ERROR_OPTICSETTINGS            -5     // 配置文件读取失败[OpticSettings]
#define  INIT_ERROR_FILEINVALID              -6     // 配置文件路径无效
/// 三态状态
#define  STATUS_INIT					-1		// 初始化
#define  STATUS_ERROR                    0		// 异常
#define  STATUS_OK						 1		// 正常
/// 触发方式
#define  TRIGGERMODE_OUT				 0		// 外触发
#define  TRIGGERMODE_FREE				 1		// 自由曝光
/// 串口信息显示模式
#define  DISPLAY_COMMINIT			     0		// 端口初始化状态显示
#define  DISPLAY_COMMRECV				 1		// 接收数据显示
#define  DISPLAY_COMMSEND				 2		// 发送数据显示
#define  DISPLAY_COMMRECVCHECK			 3      // 接收检查显示
///图片显示大小占原图百分比(*100)
#define BIGGER_FIRST                     36
#define BIGGER_SECOND                    21



