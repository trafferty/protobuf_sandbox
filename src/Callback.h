#ifndef __CALLBACK_MECHANISM_H__
#define __CALLBACK_MECHANISM_H__
//=============================================================================
//   Below is a snippet of code to do callbacks.
//
// //Format is Callback[# args]<Class Type to callback, Return value type, args types if any>.
// Callback0<CameraFLI,bool>     *cb1 = 0;
// Callback1<CameraFLI,bool,int> *cb2 = 0;
//
//   cb1 = new Callback0<CameraFLI,bool>(&camera, &CameraFLI::ExposeFrame);
//   cb2 = new Callback1<CameraFLI,bool,int>(&camera, &CameraFLI::SetExposureTime, 0);
//
//   cb1->Invoke();
//
//   cb2->Invoke();
//=============================================================================
// For this specific code one requirement is that all callbacks return a value
// of some kind. It just makes this implementation smaller.
//=============================================================================
template<typename T> union CALLBACK_DATA
{
   T data;
   char char_data[sizeof(T)];
   void *vp_data;
};

typedef char* CHAR_PTR;

//=============================================================================
// Callback Interface Class
//-----------------------------------------------------------------------------
//
//=============================================================================
class ICallback
{
 public:
   virtual ~ICallback()
   {
   }

   virtual void Invoke(void) = 0;
   virtual void Invoke(void *a1, void *a2) = 0;
   virtual void *GetReturnValue(void) = 0;
};

typedef long NULL_ARG;
typedef void (ICallback::*CALLBACK_CAST)(void);

//=============================================================================
// Callback0 Class - 0 Arguments
//-----------------------------------------------------------------------------
// Template for 0 argument callback.
//=============================================================================
template<class CLASS_TYPE, typename RETURN_TYPE>
class Callback0: public ICallback
{
 public:
   typedef RETURN_TYPE (CLASS_TYPE::* FUNCTION_TYPE0)(void);

   explicit Callback0(CLASS_TYPE *c, FUNCTION_TYPE0 f)
   {
      m_ClassType = c;
      m_MemberFunction0 = f;
      m_NumArgs = 0;
   }

   virtual void Invoke(void)
   {
      m_ReturnValue = (m_ClassType->*m_MemberFunction0)();
   }

   virtual void Invoke(void *a1, void *a2 = 0)
   {
      Invoke();
   }

   virtual void *GetReturnValue(void)
   {
      return (void *)m_ReturnValue;
   }

 protected:
   int m_NumArgs;
   int m_ArgSizeArray[4];

   CLASS_TYPE *m_ClassType;
   FUNCTION_TYPE0 m_MemberFunction0;
   RETURN_TYPE m_ReturnValue;
};

//=============================================================================
// Callback Class - 1 Arguments
//-----------------------------------------------------------------------------
// Template for 1 argument callback.
//=============================================================================
template<class CLASS_TYPE, typename RETURN_TYPE, typename ARGUMENT_1_TYPE>
class Callback1: public ICallback
{
 public:
   typedef RETURN_TYPE (CLASS_TYPE::* FUNCTION_TYPE1)(ARGUMENT_1_TYPE);
   typedef ARGUMENT_1_TYPE ARG1_CAST;

   explicit Callback1(CLASS_TYPE *c, FUNCTION_TYPE1 f, ARGUMENT_1_TYPE a1)
   {
      m_ClassType = c;
      m_MemberFunction1 = f;
      m_Argument1 = a1;
      m_NumArgs = 1;
   }

   virtual void Invoke(void)
   {
      m_ReturnValue = (m_ClassType->*m_MemberFunction1)(m_Argument1);
   }

   virtual void Invoke(void *a1, void *a2 = 0)
   {
      m_Argument1 = (ARG1_CAST) a1;
      Invoke();
   }

   virtual void *GetReturnValue(void)
   {
      return (void *)m_ReturnValue;
   }

 protected:
   long m_NumArgs;
   long m_ArgSizeArray[4];

   ARGUMENT_1_TYPE m_Argument1;
   CLASS_TYPE *m_ClassType;
   FUNCTION_TYPE1 m_MemberFunction1;
   RETURN_TYPE m_ReturnValue;
};

//=============================================================================
// Callback Class - 2 Arguments
//-----------------------------------------------------------------------------
// Template for 2 argument callback.
//=============================================================================
template<class CLASS_TYPE, typename RETURN_TYPE, typename ARGUMENT_1_TYPE,
      typename ARGUMENT_2_TYPE>
class Callback2: public ICallback
{
 public:
   typedef RETURN_TYPE (CLASS_TYPE::* FUNCTION_TYPE1)(ARGUMENT_1_TYPE, ARGUMENT_2_TYPE);
   typedef ARGUMENT_1_TYPE ARG1_CAST;
   typedef ARGUMENT_2_TYPE ARG2_CAST;

   explicit Callback2(CLASS_TYPE *c, FUNCTION_TYPE1 f, ARGUMENT_1_TYPE a1,
         ARGUMENT_2_TYPE a2)
   {
      m_ClassType = c;
      m_MemberFunction1 = f;
      m_Argument1 = a1;
      m_Argument2 = a2;
      m_NumArgs = 2;
   }

   virtual void Invoke(void)
   {
      m_ReturnValue = (m_ClassType->*m_MemberFunction1)(m_Argument1, m_Argument2);
   }

   virtual void Invoke(void *a1, void *a2 = 0)
   {
      m_Argument1 = (ARG1_CAST) a1;
      m_Argument2 = (ARG2_CAST) a2;
      Invoke();
   }

   virtual void *GetReturnValue(void)
   {
      return (void *)m_ReturnValue;
   }

 protected:
   long m_NumArgs;
   long m_ArgSizeArray[4];

   ARGUMENT_1_TYPE m_Argument1;
   ARGUMENT_2_TYPE m_Argument2;
   CLASS_TYPE *m_ClassType;
   FUNCTION_TYPE1 m_MemberFunction1;
   RETURN_TYPE m_ReturnValue;
};
#endif
