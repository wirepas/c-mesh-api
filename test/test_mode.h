/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

/**
 * \brief   Test fo entering test mode
 */
bool Set_Test_Mode(void);
/**
 * \brief   Test for exit test mode
 */
bool Exit_Test_Mode(void);
/**
 * \brief   Test for setting radio channel
 */
bool Set_Radio_Channel(void);
/**
 * \brief   Tes for setting radio TX power
 */
bool Set_Radio_Power(void);
/**
 * \brief   Test for reading maximum payload size
 */
bool Get_Radio_Max_Data_Size(uint8_t * maxLen);
/**
 * \brief   Test for sendig data over the air
 */
bool Send_Radio_Data(void);
/**
 * \brief   Test for enabling data receiving
 */
bool Enable_Radio_Reception(void);
/**
 * \brief   Test for reading last received package
 */
bool Read_Radio_Data(void);
/**
 * \brief   Test for sending predefined test signals over the air
 */
bool Send_Radio_Signal(void);