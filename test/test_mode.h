/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

/**
 * \brief   Test fo entering test mode
 */
int Set_Test_Mode(void);
/**
 * \brief   Test for exit test mode
 */
int Exit_Test_Mode(void);
/**
 * \brief   Test for setting radio channel
 */
int Set_Radio_Channel(void);
/**
 * \brief   Tes for setting radio TX power
 */
int Set_Radio_Power(void);
/**
 * \brief   Test for reading maximum payload size
 */
int Get_Radio_Max_Data_Size(uint8_t * maxLen);
/**
 * \brief   Test for sendig data over the air
 */
int Send_Radio_Data(void);
/**
 * \brief   Test for enabling data receiving
 */
int Enable_Radio_Reception(void);
/**
 * \brief   Test for reading last received package
 */
int Read_Radio_Data(void);
/**
 * \brief   Test for sending predefined test signals over the air
 */
int Send_Radio_Signal(void);