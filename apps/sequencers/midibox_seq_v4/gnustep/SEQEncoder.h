// $Id: SEQEncoder.h 41 2008-09-30 17:20:12Z tk $
//
//  SEQEncoder.h
//  midibox_seq_v4
//
//  Created by Thorsten Klose on 30.09.08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@interface SEQEncoder : NSSlider {

}

- (IBAction)encoderMoved:(id)sender;

@end