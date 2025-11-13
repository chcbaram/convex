#include "hw_def.h"




#include "cli.h"
#include "log.h"
#include "reset.h"

#define ASSERT_LOG_MAX_LOG      4
#define ASSERT_LOG_NAME_LEN     64
#define ASSERT_LOG_EXPR_LEN     64

typedef enum
{
  ASSERT_TYPE_STM32,
  ASSERT_TYPE_FW,
  ASSERT_TYPE_SYS,
  ASSERT_TYPE_MAX
} AssertType_t;

typedef struct 
{
  AssertType_t type;
  char         name[ASSERT_LOG_NAME_LEN];
  char         expr[ASSERT_LOG_EXPR_LEN];
  uint32_t     line;
} assert_info_t;


#if CLI_USE(HW_ASSERT)
static void cliAssert(cli_args_t *args);
#endif
static void assertProcessFailed(uint8_t* file, uint32_t line, uint8_t *expr, AssertType_t type);
void __assert_func(const char *file, int line, const char *func, const char *expr);

static assert_info_t fail_log[ASSERT_LOG_MAX_LOG];
static const char *fail_type_str[ASSERT_TYPE_MAX] = {"TYPE_STM32", "TYPE_FW", "TYPE_SYS"};

static bool is_init = false;
static uint16_t fail_cnt = 0;






bool assertInit(void)
{
  is_init = true;


#if CLI_USE(HW_ASSERT)
  cliAdd("assert", cliAssert);
#endif  
  return true;
}


void assertFailed(uint8_t *file, uint32_t line, uint8_t *expr)
{
  assertProcessFailed(file, line, expr, ASSERT_TYPE_FW);
}

void assert_failed(uint8_t* file, uint32_t line)
{
  assertProcessFailed(file, line, NULL, ASSERT_TYPE_STM32);
}

void assertProcessFailed(uint8_t* file, uint32_t line, uint8_t *expr, AssertType_t type)
{
  if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)  
  { 
    __BKPT(0);
  }

  char *name_buf;

  if (strrchr((char *) file,'/') == NULL) 
  {
    name_buf = strrchr((char *)file,'\\')+1;
  }
  else 
  {
    name_buf = strrchr((char *)file,'/')+1;
  }

  if (fail_cnt < ASSERT_LOG_MAX_LOG)
  {
    uint32_t index = fail_cnt;

    fail_log[index].type = type;
    strncpy(fail_log[index].name, name_buf, ASSERT_LOG_NAME_LEN);
    if (expr != NULL)
    {
      strncpy(fail_log[index].expr, (char *)expr, ASSERT_LOG_EXPR_LEN);
    }
    fail_log[index].line = line;    

    if (logIsOpen())
    {
      logPrintf("assert found: %d\n", fail_cnt);
      logPrintf("     - type : %s\n", type < ASSERT_TYPE_MAX ? fail_type_str[type]:"");
      logPrintf("     - file : %s\n", name_buf);
      logPrintf("     - line : %d\n", line);
      logPrintf("     - expr : (%s)\n", fail_log[index].expr);
    }

    fail_cnt++;
  }
}

void __assertion_failed(char *_Expr)
{
  if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)  
  { 
    __BKPT(0);
  }  
}

void __assert_func(const char *file, int line, const char *func, const char *expr)
{  
  AssertType_t type = ASSERT_TYPE_SYS;

  if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)  
  { 
    __BKPT(0);
  }  

  char *name_buf;

  if (strrchr((char *) file,'/') == NULL) 
  {
    name_buf = strrchr((char *)file,'\\')+1;
  }
  else 
  {
    name_buf = strrchr((char *)file,'/')+1;
  }

  if (fail_cnt < ASSERT_LOG_MAX_LOG)
  {
    uint32_t index = fail_cnt;

    fail_log[index].type = type;
    strncpy(fail_log[index].name, name_buf, ASSERT_LOG_NAME_LEN);
    if (expr != NULL)
    {
      strncpy(fail_log[index].expr, (char *)expr, ASSERT_LOG_EXPR_LEN);
    }
    fail_log[index].line = line;    

    if (logIsOpen())
    {
      logPrintf("assert found: %d\n", fail_cnt);
      logPrintf("     - type : %s\n", type < ASSERT_TYPE_MAX ? fail_type_str[type]:"");
      logPrintf("     - file : %s\n", name_buf);
      logPrintf("     - func : %s\n", func);
      logPrintf("     - line : %d\n", line);
      logPrintf("     - expr : (%s)\n", fail_log[index].expr);
    }

    fail_cnt++;
  }  

  resetToBoot();
  
  while(1);
}


