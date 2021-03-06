//////////////////////////////////////////////////////////////////////////////
// sixty four pixels 2020                                       CC-NC-BY-SA //
//                                //  //          //                        //
//   //////   /////   /////   //////  //   /////  //////   /////  //   //   //
//   //   // //   // //   // //   //  //  //   // //   // //   //  // //    //
//   //   // //   // //   // //   //  //  /////// //   // //   //   ///     //
//   //   // //   // //   // //   //  //  //      //   // //   //  // //    //
//   //   //  /////   /////   //////   //  /////  //////   /////  //   //   //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//                                                                          //
// LED DEFINITIONS
//                                                                          //
//////////////////////////////////////////////////////////////////////////////
#ifndef LEDS_H_
#define LEDS_H_

CPulseOut g_gate_led(kGPIO_PORTB, 5);
CPulseOut g_tempo_led(kGPIO_PORTC, 2);
CPulseOut g_midi_led(kGPIO_PORTC, 3);

#endif /* LEDS_H_ */
