# 原始需求

## 整体说明
1 我需要开发一个单纯由markdown文档组成的skill，名字叫api-finder。 在当前路径创建一个api-finder文件夹，在文件夹内部创建SKILL.md文档。承载接下来的开发内容。
2 核心功能: 识别出目标项目中，哪些函数是“外部接口”。“外部接口”定义：本项目模块定义的函数，作用是提供给外部用户或外部模块与本项目模块进行通信交互。我们更关心向本项目模块传入外部（不可信）数据的外部接口。

## 流程与架构
### 工具输入
1 SKILL使用方法：/api-finder <project_dir>
2 确定代码分析范围
	2.1 参数project_dir为待分析项目路径，如果用户没有指定该参数，默认为当前文件夹。
    2.2 在project_dir/.ethunter_out中，如果存在clean_code.txt文件。 则分析范围为clean_code中记录的全部文件。 clean_code.txt中用纯文本的形式记录了多个文件的绝对路径。每一行一个文件。
    '''
    /srv/workspace/work_code/src/a/b.c
    /srv/workspace/work_code/src/a/b.h
    '''
 	2.3 在project_dir/.ethunter_out中，如果不存在clean_code.txt文件，但存在.etignore文件。 分析范围为project_dir中全部.c和.h文件，但需要将.etignore文件中指定的子目录或文件排除掉，不进行分析。.etignore文件语法和.gitignore语法一样。
 	2.4 在project_dir/.ethunter_out中，如果不存在clean_code.txt和.etignore文件，分析范围为project_dir中全部.c和.h文件。
    2.5 预加载编译宏配置：在project_dir/.ethunter_out中，如果存在macro.json文件，记住方便后续直接读取（格式 {"MACRO": "value", ...}），这些是编译指令中动态定义的宏，可能影响分析的范围（有些代码由宏开关#ifdef控制）。若不存在则跳过，后续照常分析。
3 初始化输出路径，没有文件夹则创建：project_dir/.ethunter_out/api-finder
4 检查project_dir/.ethunter_out/api-finder/conf/old_api.json是否存在。 该json记录了历史版本中识别出来的外部接口。 存在则记住方便后续直接读取，不存在则跳过继续后面分析。 格式为
'''
[
	{
		"name": "FUNC_NAME",
		"file": "FILE_PATH"
	}，
	{
		"name": "FUNC_NAME2",
		"file": "FILE_PATH2"
	}
]
'''
5 检查project_dir/.ethunter_out/api-finder/conf/black_api.json是否存在。该json记录了历史版本中识别出来的非外部接口。存在则记住方便后续直接读取，不存在则跳过继续后面分析。格式和old_api.json一致。
6 检查project_dir/.codegraph是否存在。 如果存在，在后续的代码分析中，优先使用mcp工具codegraph提供的工具进行代码分析。如果不存在，则使用默认的代码仓探索工具进行分析。

### 任务恢复
如果之前分析异常中断，根据已有的中间结果，resume分析。

### 了解项目架构
1 该步骤主要功能是了解待分析项目的架构设计。了解本项目模块的具体功能，进而推测出本项目模块哪部分代码会和外部模块进行通信，会和哪些外部模块通信。 这里注意，分析目标仅有本项目模块代码，并没有外部模块的代码。因此需要合理推测。
2 利用LLM的代码分析能力来实现项目分析任务，分析结果保存在project_dir/.ethunter_out/api-finder/arch.md
3 在分析前，需要检查arch.md是否存在，如果存在则跳过该步骤

### 识别外部接口
1 接口继承： 
	1.1 如果old_api.json存在，分析这些历史版本的外部接口是否包含在当前的代码分析范围中（有可能被删除或在非目标文件中）。整理全部包含在当前分析范围内的函数列表，作为继承的外部接口列表。  如果old_api.json不存在，直接执行步骤3。
2 接口特征提取： 
	2.1 如果存在继承接口列表，逐个分析继承接口的注册特征。
		一个函数被注册成外部接口的方式就是将该函数与一个**全局handler(函数指针)**进行绑定,注册的方式可以分为静态注册和动态注册两种:
		- 静态注册:在**全局变量的定义阶段**完成接口函数与handler绑定

		```c
		AT_DDR_DATA STATIC ap_msg_handle_func push_conn_func_table[] = {
		        {PUSH_START_PROXY_REQ, start_push_proxy}, //start_push_proxy被赋值给了ap_msg_handle_func结构体中的handler
		    	{PUSH_STOP_PROXY_REQ, stop_push_proxy}, //stop_push_proxy被赋值给了ap_msg_handle_func结构体中的handler
		    	...
		};
		```
			在该例子中，全局静态数组push_conn_func_table的定义即一种特征，如果start_push_proxy是外部接口，我们可以合理推测，stop_push_proxy也是一个外部接口。类似的，这种注册的形式也可能是一个全局静态的结构体定义。全局静态函数的定义等等。 


		- 动态注册:在**代码执行过程中**完成接口函数与handler绑定

		```c
		AT_DDR_TEXT void ai_svc_mail_dispatcher_register_wrapper(UINT8 register_cmd, mail_process_cb register_cb, UINT8 register_need_vote_ddr)
		{
		        struct mail_process_cb_node cb_node = {
		                .cb = register_cb,
		                .cmd = register_cmd,
		                .need_vote_ddr = register_need_vote_ddr,
		        };
		        ai_svc_mail_dispatcher_register(&cb_node);
		}

		ai_svc_mail_dispatcher_register_wrapper(AI_SVC_FUSION_SETS, ai_svc_fusion_sets_handler, NEED_VOTE_DDR);
		//在注册函数ai_svc_mail_dispatcher_register_wrapper被调用时,ai_svc_fusion_sets_handler被赋值给mail_process_cb_node结构体中的hadnler
		```
			在该例子中，ai_svc_mail_dispatcher_register_wrapper是一个注册函数，也可以当作一个特征。如果ai_svc_fusion_sets_handler是外部接口。那么如果其它代码用ai_svc_mail_dispatcher_register_wrapper注册了另一个函数ai_svc_fusion_gets_handler, 我们也可以合理推测也是一个外部接口。

	2.2 分析单个接口函数注册特征的方法：
		- 找到该接口函数在代码中的使用点
		- 依次排查使用点:如果该使用点不是正常的函数调用,而是将**接口函数作为参数传递给注册函数**或者将**接口函数赋值给某个handler(函数指针)**,那么该处使用点即接口注册点
		- 继续追踪注册函数的实现代码,或handler的实现代码,找到接口被绑定的handler定义。至此就了解了完整的注册逻辑。

	2.3 根据继承外部接口的注册特征，识别代码分析范围中其它符合这些特征的函数。 并用arch.md中的信息，验证其是否为外部接口，可能在与哪个外部模块进行通信。 将确定为外部接口的函数添加到特征识别接口列表。


3 根据arch.md中的项目架构信息，找到本项目模块与外部模块通信的部分，根据代码语义识别出外部接口。
外部接口有两种形式：
- external inputs from parameters：

  - This type of interface is usually registered as a handler and provided for other modules to call. It is typically not called by the code of this project.

  ```c
  void get_user_input(void* data, int size){
  	//parameter data and size are external inputs    
  }
  ```
  

- external inputs from shared memory, files, or IPC channels

  - This type of interface typically directly calls specific functions to read data from shared memory, files or IPCs.

  ```c
  void get_user_input(){
      ...
      UINT32 buffer[MAX_SIZE];
      read_from_shared_memory(buffer);
      //buffer is external input from shared memory
      ...
  }
  ```
结合这两种形式，根据代码语义，找到全部符合条件的外部接口，整理成架构识别接口列表

4 整理最终外部接口列表： 继承接口列表 + 特征识别接口列表 + 架构识别接口列表，合并之后去重。

### 外部接口筛选
1 黑名单筛选：如果project_dir/.ethunter_out/black_api.json存在，读取黑名单列表，当函数名和文件名一致时，排除该外部接口。

2 规则筛选
	2.1 用于测试的函数排除
	2.2 没有参数的函数排除
	2.3 冗余函数排除（可以根据代码语义和注释等信息推测）


### 最终结果输出
1 project_dir/.ethunter_out/api-finder/api.json，格式和old_api.json一致

2 project_dir/.ethunter_out/api-finder/summary.md，按照api.json中的顺序，逐个说明识别为api的理由



## 要求
1 需要使用代码仓探索的常用工具，SKILL应提前申请权限。 当project_dir/.codegraph存在时，还需要使用mcp工具codegraph，但如果环境中没有安装codegraph或没配置mcp，不要中断分析，使用常规工具即可。
2 整个skill的提示词都用中文。 注意相同语义的用词前后保持一致。
3 本skill包含长任务（多个待分析接口），提示词应通过规范方式防止任务遗忘，必要时要保存中间结果，方便断点续分析。
4 全部任务由主agent完成，不需要sub-agent并行分析。 
5 需求只写了简单的分析方法描述，可以帮我完善下分析方法
6 skill适用业务：适用于嵌入式/底层系统代码（Linux内核驱动、UEFI/BL31/BL2/XLoader、ISP/SensorHub/GPU固件）。